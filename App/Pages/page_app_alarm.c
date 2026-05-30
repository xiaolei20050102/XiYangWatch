#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

/* ── 视图 ── */
static lv_obj_t *view_list;
static lv_obj_t *view_edit;
static lv_obj_t *list_cont;   /* 列表滚动容器 */

/* ── 本地闹钟存储（模拟器 stub 替代） ── */
#define ALARM_MAX 10
static watch_alarm_t alarms[ALARM_MAX];
static int alarm_count;

/* ── 编辑中的闹钟数据 ── */
static watch_alarm_t edit_buf;
static int edit_idx;          /* -1 = 新增 */
static lv_obj_t *lbl_repeat_val;
static int repeat_mode;       /* 0=Never 1=Everyday 2=Weekdays 3=Weekends */
static bool edit_enable;
static bool s_swiping; /* 防止滑动时误触点击 */

/* ── 滚轮选择器 ── */
#define ROLLER_W     80
#define ROLLER_ROW_H  36

static lv_obj_t *roller_hour;
static lv_obj_t *roller_min;

static const char hour_opts[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
    "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
    "20\n21\n22\n23";
static const char min_opts[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
    "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
    "20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n"
    "30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n"
    "40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n"
    "50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

static const char *repeat_names[] = {
    "Never", "Everyday", "Weekdays", "Weekends"
};

/* ── 辅助 ── */
static lv_obj_t *mk_label(lv_obj_t *p)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    return l;
}

static const char *repeat_to_str(const bool r[7])
{
    int s = 0; for (int i = 0; i < 7; i++) if (r[i]) s |= (1 << i);
    if (s == 0x7F) return "Everyday";
    if (s == 0x3E) return "Weekdays";   /* Mon-Fri */
    if (s == 0x41) return "Weekends";   /* Sun+Sat */
    return "Custom";
}

static void set_repeat(bool r[7], int mode)
{
    switch (mode) {
    case 0: /* Never */   for (int i = 0; i < 7; i++) r[i] = false; break;
    case 1: /* Everyday */ for (int i = 0; i < 7; i++) r[i] = true;  break;
    case 2: /* Weekdays */ r[0]=false; r[1]=r[2]=r[3]=r[4]=r[5]=true; r[6]=false; break;
    case 3: /* Weekends */ r[0]=true; r[1]=r[2]=r[3]=r[4]=r[5]=false; r[6]=true; break;
    }
}

static void on_list_click(lv_event_t *e);
static void on_switch_changed(lv_event_t *e);
static void build_edit_view(lv_obj_t *root);
static void destroy_edit_view(void);

/* ══════ 列表视图 ══════ */
static void refresh_list(void)
{
    lv_obj_clean(list_cont);
    for (int i = 0; i < alarm_count; i++) {
        watch_alarm_t a = alarms[i];

        lv_obj_t *card = lv_obj_create(list_cont);
        lv_obj_set_size(card, 210, 56);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);

        /* 时间 */
        lv_obj_t *time = mk_label(card);
        lv_label_set_text_fmt(time, "%02d:%02d", a.hour, a.min);
        lv_obj_set_style_text_font(time, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(time, a.enabled
            ? lv_color_white() : lv_color_hex(0x444444), 0);
        lv_obj_align(time, LV_ALIGN_LEFT_MID, 14, -6);

        /* 重复 */
        lv_obj_t *rep = mk_label(card);
        lv_label_set_text(rep, repeat_to_str(a.repeat));
        lv_obj_set_style_text_font(rep, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(rep, lv_color_hex(0x777777), 0);
        lv_obj_align(rep, LV_ALIGN_LEFT_MID, 14, 14);

        /* 开关 */
        lv_obj_t *sw = lv_obj_create(card);
        lv_obj_set_size(sw, 60, 28);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_radius(sw, 14, 0);
        lv_obj_set_style_pad_all(sw, 0, 0);
        lv_obj_set_style_border_width(sw, 0, 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(sw,
            a.enabled ? lv_color_hex(0x00C853) : lv_color_hex(0xCCCCCC), 0);

        lv_obj_t *sw_lbl = lv_label_create(sw);
        lv_label_set_text(sw_lbl, a.enabled ? "ON" : "OFF");
        lv_obj_set_style_text_font(sw_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(sw_lbl,
            a.enabled ? lv_color_white() : lv_color_black(), 0);
        lv_obj_center(sw_lbl);

        lv_obj_set_user_data(sw, (void *)(intptr_t)i);
        lv_obj_add_event_cb(sw, on_switch_changed, LV_EVENT_CLICKED, NULL);

        /* 点击事件 */
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(card, (void *)(intptr_t)i);
        lv_obj_add_event_cb(card, on_list_click, LV_EVENT_CLICKED, NULL);
    }
}

static void on_switch_changed(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(sw);
    alarms[idx].enabled = !alarms[idx].enabled;
    bool on = alarms[idx].enabled;
    lv_obj_set_style_bg_color(sw,
        on ? lv_color_hex(0x00C853) : lv_color_hex(0xCCCCCC), 0);
    lv_obj_t *sw_lbl = lv_obj_get_child(sw, 0);
    lv_label_set_text(sw_lbl, on ? "ON" : "OFF");
    lv_obj_set_style_text_color(sw_lbl,
        on ? lv_color_white() : lv_color_black(), 0);
    lv_obj_t *card = lv_obj_get_parent(sw);
    lv_obj_t *time_label = lv_obj_get_child(card, 0);
    lv_obj_set_style_text_color(time_label,
        on ? lv_color_white() : lv_color_hex(0x444444), 0);
}

static void on_list_click(lv_event_t *e)
{
    if (s_swiping) { s_swiping = false; return; }
    lv_obj_t *card = lv_event_get_current_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(card);
    edit_buf = alarms[idx];
    edit_idx = idx;
    edit_enable = edit_buf.enabled;
    /* 推断 repeat mode */
    int s = 0; for (int i = 0; i < 7; i++) if (edit_buf.repeat[i]) s |= (1 << i);
    if (s == 0x7F) repeat_mode = 1;
    else if (s == 0x3E) repeat_mode = 2;
    else if (s == 0x41) repeat_mode = 3;
    else repeat_mode = 0;

    if (view_edit == NULL) build_edit_view(lv_obj_get_parent(view_list));
    lv_label_set_text(lbl_repeat_val, repeat_names[repeat_mode]);

    lv_obj_add_flag(view_list, LV_OBJ_FLAG_HIDDEN);

    lv_roller_set_selected(roller_hour, edit_buf.hour, LV_ANIM_OFF);
    lv_roller_set_selected(roller_min, edit_buf.min, LV_ANIM_OFF);
}

static void on_fab(lv_event_t *e)
{
    (void)e;
    if (alarm_count >= ALARM_MAX) return;
    edit_idx = -1;
    edit_buf.hour = 7; edit_buf.min = 0;
    edit_buf.enabled = true;
    set_repeat(edit_buf.repeat, 1);
    repeat_mode = 1; edit_enable = true;

    if (view_edit == NULL) build_edit_view(lv_obj_get_parent(view_list));
    lv_label_set_text(lbl_repeat_val, repeat_names[1]);

    lv_obj_add_flag(view_list, LV_OBJ_FLAG_HIDDEN);

    lv_roller_set_selected(roller_hour, 7, LV_ANIM_OFF);
    lv_roller_set_selected(roller_min, 0, LV_ANIM_OFF);
}

/* ══════ 编辑视图 ══════ */

static void on_repeat_toggle(lv_event_t *e)
{
    (void)e;
    repeat_mode = (repeat_mode + 1) % 4;
    lv_label_set_text(lbl_repeat_val, repeat_names[repeat_mode]);
}

static void on_save(lv_event_t *e)
{
    (void)e;
    edit_buf.hour = lv_roller_get_selected(roller_hour);
    edit_buf.min  = lv_roller_get_selected(roller_min);
    edit_buf.enabled = edit_enable;
    set_repeat(edit_buf.repeat, repeat_mode);
    if (edit_idx < 0) {
        if (alarm_count < ALARM_MAX) {
            edit_idx = alarm_count;
            alarm_count++;
        } else {
            return; /* 满了 */
        }
    }
    alarms[edit_idx] = edit_buf;
    refresh_list();
    lv_obj_remove_flag(view_list, LV_OBJ_FLAG_HIDDEN);
    destroy_edit_view();
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    if (edit_idx >= 0) {
        for (int i = edit_idx; i < alarm_count - 1; i++)
            alarms[i] = alarms[i + 1];
        alarm_count--;
    }
    refresh_list();
    lv_obj_remove_flag(view_list, LV_OBJ_FLAG_HIDDEN);
    destroy_edit_view();
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    lv_obj_remove_flag(view_list, LV_OBJ_FLAG_HIDDEN);
    destroy_edit_view();
}

static void build_edit_view(lv_obj_t *root)
{
    view_edit = lv_obj_create(root);
    lv_obj_set_size(view_edit, 240, 280);
    lv_obj_set_style_pad_all(view_edit, 0, 0);
    lv_obj_set_style_border_width(view_edit, 0, 0);
    lv_obj_set_style_bg_color(view_edit, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(view_edit, LV_OPA_COVER, 0);

    /* ── 滚轮时间选择器 ── */
    roller_hour = lv_roller_create(view_edit);
    lv_roller_set_options(roller_hour, hour_opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_hour, 3);
    lv_obj_set_size(roller_hour, ROLLER_W, ROLLER_ROW_H * 3);
    lv_obj_set_pos(roller_hour, 28, 36);
    lv_obj_set_style_text_font(roller_hour, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller_hour, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(roller_hour, &lv_font_montserrat_28, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller_hour, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller_hour, lv_color_hex(0x1A1A1A), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller_hour, LV_OPA_20, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller_hour, 8, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller_hour, 0, 0);
    lv_obj_set_style_bg_opa(roller_hour, LV_OPA_TRANSP, 0);

    roller_min = lv_roller_create(view_edit);
    lv_roller_set_options(roller_min, min_opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_min, 3);
    lv_obj_set_size(roller_min, ROLLER_W, ROLLER_ROW_H * 3);
    lv_obj_set_pos(roller_min, 132, 36);
    lv_obj_set_style_text_font(roller_min, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller_min, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(roller_min, &lv_font_montserrat_28, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller_min, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller_min, lv_color_hex(0x1A1A1A), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller_min, LV_OPA_20, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller_min, 8, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller_min, 0, 0);
    lv_obj_set_style_bg_opa(roller_min, LV_OPA_TRANSP, 0);

    /* 冒号分隔 */
    lv_obj_t *col = mk_label(view_edit);
    lv_label_set_text(col, ":");
    lv_obj_set_style_text_font(col, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(col, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, -62);

    /* ── Repeat 卡片：居中于选择器和按钮之间 ── */
    lv_coord_t opt_y = 170;
    const int row_h = 36;

    /* Repeat 卡片 */
    {
        lv_obj_t *card = lv_obj_create(view_edit);
        lv_obj_set_size(card, 200, row_h + 8);
        lv_obj_set_pos(card, 20, opt_y - 4);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *rl = mk_label(card);
        lv_label_set_text(rl, "Repeat");
        lv_obj_set_style_text_font(rl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(rl, lv_color_white(), 0);
        lv_obj_align(rl, LV_ALIGN_LEFT_MID, 12, 0);

        lbl_repeat_val = mk_label(card);
        lv_label_set_text(lbl_repeat_val, "Everyday");
        lv_obj_set_style_text_font(lbl_repeat_val, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_repeat_val, lv_color_hex(0x888888), 0);
        lv_obj_align(lbl_repeat_val, LV_ALIGN_RIGHT_MID, -12, 0);

        lv_obj_add_event_cb(card, on_repeat_toggle, LV_EVENT_CLICKED, NULL);
    }

    /* ── 底部按钮：水平排列 ── */
    {
        lv_obj_t *b;
        lv_obj_t *l;

        b = lv_obj_create(view_edit);
        lv_obj_set_size(b, 104, 40);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, -56, -16);
        lv_obj_set_style_radius(b, 20, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x00C853), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        l = lv_label_create(b);
        lv_label_set_text(l, "Save");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, on_save, LV_EVENT_CLICKED, NULL);

        b = lv_obj_create(view_edit);
        lv_obj_set_size(b, 104, 40);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 56, -16);
        lv_obj_set_style_radius(b, 20, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xFF3D00), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        l = lv_label_create(b);
        lv_label_set_text(l, "Delete");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, on_delete, LV_EVENT_CLICKED, NULL);
    }

}

static void destroy_edit_view(void)
{
    if (view_edit) {
        lv_obj_delete(view_edit);
        view_edit = NULL;
        roller_hour = NULL;
        roller_min = NULL;
        lbl_repeat_val = NULL;
    }
}

/* ══════ 手势 ─══════ */
static bool gesture_intercept(gesture_t g)
{
    /* 编辑视图：左滑/右滑 → 保存退出 */
    if (view_edit != NULL) {
        if (g == GESTURE_LEFT || g == GESTURE_RIGHT) {
            on_save(NULL);
            return true;
        }
        return false;
    }
    /* 列表视图：左滑/右滑退出 */
    if (g == GESTURE_LEFT || g == GESTURE_RIGHT) {
        s_swiping = true;
        page_manager_pop(GESTURE_RIGHT);
        return true;
    }
    return false;
}

/* ══════ 主入口 ─═════ */
static lv_obj_t *create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ── 列表视图 ── */
    view_list = lv_obj_create(root);
    lv_obj_set_size(view_list, 240, 280);
    lv_obj_set_style_pad_all(view_list, 0, 0);
    lv_obj_set_style_border_width(view_list, 0, 0);
    lv_obj_set_style_bg_opa(view_list, LV_OPA_TRANSP, 0);

    /* 列表 */
    list_cont = lv_obj_create(view_list);
    lv_obj_set_size(list_cont, 220, 228);
    lv_obj_set_style_pad_all(list_cont, 0, 0);
    lv_obj_set_style_pad_row(list_cont, 8, 0);
    lv_obj_set_style_border_width(list_cont, 0, 0);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
    lv_obj_set_style_pad_right(list_cont, 4, 0);
    lv_obj_set_style_bg_color(list_cont, lv_color_hex(0x333333), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list_cont, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(list_cont, 3, LV_PART_SCROLLBAR);
    lv_obj_align(list_cont, LV_ALIGN_TOP_MID, 0, 26);

    refresh_list();

    /* FAB + */
    lv_obj_t *fab = lv_obj_create(view_list);
    lv_obj_set_size(fab, 42, 42);
    lv_obj_set_style_radius(fab, 21, 0);
    lv_obj_set_style_bg_color(fab, lv_color_hex(0x00C853), 0);
    lv_obj_set_style_bg_opa(fab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fab, 0, 0);
    lv_obj_set_scrollbar_mode(fab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(fab, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_flag(fab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *fab_lbl = lv_label_create(fab);
    lv_label_set_text(fab_lbl, "+");
    lv_obj_set_style_text_font(fab_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(fab_lbl, lv_color_white(), 0);
    lv_obj_center(fab_lbl);
    lv_obj_add_event_cb(fab, on_fab, LV_EVENT_CLICKED, NULL);

    view_edit = NULL;

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
    destroy_edit_view();
}

const page_t page_app_alarm = {
    .name    = "alarm",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = NULL,
};
