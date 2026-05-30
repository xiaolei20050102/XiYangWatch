#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

/* ── 状态机 ── */
typedef enum { IDLE, RUNNING, PAUSED } state_t;
static state_t s_state;

static lv_obj_t *label_timer;
static lv_obj_t *label_status;
static lv_obj_t *btn_left;
static lv_obj_t *btn_right;
static lv_obj_t *lbl_left;
static lv_obj_t *lbl_right;
static lv_obj_t *lap_cont;

static lv_timer_t *tick_timer;
static uint32_t elapsed_ms;

/* ── 计圈：定长循环队列 ── */
#define LAP_CAP 50
static uint32_t lap_times[LAP_CAP];
static int lap_head;   /* 队首（最早记录）下标 */
static int lap_cnt;    /* 当前队列长度 0..LAP_CAP */
static int lap_seq;    /* 下一个要分配的编号 */

extern const lv_font_t montserrat_48_digits;

/* ── 格式化 MM:SS:CC ── */
static void fmt_time(uint32_t ms, char *buf)
{
    uint32_t m = ms / 60000;
    uint32_t s = (ms / 1000) % 60;
    uint32_t c = (ms / 10) % 100;
    buf[0] = '0' + m / 10;
    buf[1] = '0' + m % 10;
    buf[2] = ':';
    buf[3] = '0' + s / 10;
    buf[4] = '0' + s % 10;
    buf[5] = ':';
    buf[6] = '0' + c / 10;
    buf[7] = '0' + c % 10;
    buf[8] = '\0';
}

static void refresh_display(void)
{
    char buf[16];
    fmt_time(elapsed_ms, buf);
    lv_label_set_text(label_timer, buf);
}

static void refresh_laps(void)
{
    lv_obj_clean(lap_cont);
    /* 从最新到最旧遍历队列 */
    for (int i = 0; i < lap_cnt; i++) {
        int idx = (lap_head + lap_cnt - 1 - i) % LAP_CAP;
        int num = lap_seq - 1 - i;
        char tbuf[16];
        fmt_time(lap_times[idx], tbuf);
        lv_label_set_text_fmt(lv_label_create(lap_cont),
                              "Lap %d   %s", num, tbuf);
    }
    for (int i = 0; i < lv_obj_get_child_cnt(lap_cont); i++) {
        lv_obj_t *c = lv_obj_get_child(lap_cont, i);
        lv_obj_set_style_text_font(c, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(c, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_set_width(c, 200);
    }
}

static void update_buttons(void)
{
    switch (s_state) {
    case IDLE:
        lv_label_set_text(lbl_left, "Reset");
        lv_label_set_text(lbl_right, "Start");
        lv_obj_set_style_bg_color(btn_right, lv_color_hex(0x00C853), 0);
        break;
    case RUNNING:
        lv_label_set_text(lbl_left, "Lap");
        lv_label_set_text(lbl_right, "Stop");
        lv_obj_set_style_bg_color(btn_right, lv_color_hex(0xFF3D00), 0);
        break;
    case PAUSED:
        lv_label_set_text(lbl_left, "Reset");
        lv_label_set_text(lbl_right, "Resume");
        lv_obj_set_style_bg_color(btn_right, lv_color_hex(0x00C853), 0);
        break;
    }
}

/* ── 定时器回调 50ms ── */
static void on_tick(lv_timer_t *t)
{
    (void)t;
    elapsed_ms += 50;
    refresh_display();
}

/* ── 按钮回调 ── */
static void on_left(lv_event_t *e)
{
    (void)e;
    if (s_state == RUNNING) {
        /* 入队：满则覆盖队首（最旧记录） */
        if (lap_cnt == LAP_CAP) {
            lap_times[lap_head] = elapsed_ms;
            lap_head = (lap_head + 1) % LAP_CAP;
        } else {
            int tail = (lap_head + lap_cnt) % LAP_CAP;
            lap_times[tail] = elapsed_ms;
            lap_cnt++;
        }
        lap_seq++;
        refresh_laps();
    } else {
        elapsed_ms = 0;
        lap_head = 0;
        lap_cnt = 0;
        lap_seq = 1;
        refresh_display();
        lv_obj_clean(lap_cont);
        if (s_state == PAUSED) {
            s_state = IDLE;
            lv_label_set_text(label_status, "Ready");
            update_buttons();
        }
    }
}

static void on_right(lv_event_t *e)
{
    (void)e;
    switch (s_state) {
    case IDLE:
        s_state = RUNNING;
        tick_timer = lv_timer_create(on_tick, 50, NULL);
        lv_label_set_text(label_status, "Running");
        break;
    case RUNNING:
        s_state = PAUSED;
        if (tick_timer) { lv_timer_delete(tick_timer); tick_timer = NULL; }
        lv_label_set_text(label_status, "Paused");
        break;
    case PAUSED:
        s_state = RUNNING;
        tick_timer = lv_timer_create(on_tick, 50, NULL);
        lv_label_set_text(label_status, "Running");
        break;
    }
    update_buttons();
}

/* ── 创建按钮 ── */
static lv_obj_t *make_btn(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           const char *text, lv_color_t bg, lv_obj_t **out_lbl)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 100, 44);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 22, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    *out_lbl = lbl;
    return btn;
}

/* ── 手势：右滑返回菜单 ── */
static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_RIGHT) {
        if (tick_timer) { lv_timer_delete(tick_timer); tick_timer = NULL; }
        page_manager_pop(GESTURE_RIGHT);
        return true;
    }
    return false;
}

static lv_obj_t *create(lv_obj_t *parent)
{
    s_state = IDLE;
    elapsed_ms = 0;
    lap_head = 0;
    lap_cnt = 0;
    lap_seq = 1;
    tick_timer = NULL;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ── 计时器 MM:SS:CC ── */
    label_timer = lv_label_create(root);
    lv_label_set_text(label_timer, "00:00:00");
    lv_obj_set_style_text_font(label_timer, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_timer, lv_color_white(), 0);
    lv_obj_align(label_timer, LV_ALIGN_CENTER, 0, -60);

    /* ── 状态 ── */
    label_status = lv_label_create(root);
    lv_label_set_text(label_status, "Ready");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x666666), 0);
    lv_obj_align_to(label_status, label_timer, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    /* ── 计圈列表 ── */
    lap_cont = lv_obj_create(root);
    lv_obj_set_size(lap_cont, 210, 96);
    lv_obj_set_style_pad_all(lap_cont, 4, 0);
    lv_obj_set_style_pad_row(lap_cont, 2, 0);
    lv_obj_set_style_border_width(lap_cont, 1, 0);
    lv_obj_set_style_border_color(lap_cont, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(lap_cont, 10, 0);
    lv_obj_set_style_bg_opa(lap_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(lap_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lap_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(lap_cont, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(lap_cont, LV_DIR_VER);
    lv_obj_set_style_bg_color(lap_cont, lv_color_hex(0x333333), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(lap_cont, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(lap_cont, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(lap_cont, 3, LV_PART_SCROLLBAR);
    lv_obj_align(lap_cont, LV_ALIGN_CENTER, 0, 30);

    /* ── 底部按钮 ── */
    lv_coord_t by = 228;
    btn_left  = make_btn(root, 14, by, "Reset", lv_color_hex(0x1C1C1E), &lbl_left);
    btn_right = make_btn(root, 126, by, "Start", lv_color_hex(0x00C853), &lbl_right);

    lv_obj_add_event_cb(btn_left, on_left, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_right, on_right, LV_EVENT_CLICKED, NULL);

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
    if (tick_timer) { lv_timer_delete(tick_timer); tick_timer = NULL; }
}

const page_t page_app_stopwatch = {
    .name    = "stopwatch",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = NULL,
};
