#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_time;
static lv_obj_t *label_date_top;
static lv_obj_t *label_date_bot;
static lv_obj_t *status_arc;
static lv_obj_t *label_status_num;
static lv_obj_t *health_card;
static lv_obj_t *health_icon;
static lv_obj_t *health_title;
static lv_obj_t *health_subtitle;

static lv_obj_t *ring_hr;
static lv_obj_t *ring_alt;
static lv_obj_t *ring_steps;
static lv_obj_t *label_hr_val;
static lv_obj_t *label_hr_unit;
static lv_obj_t *label_alt_val;
static lv_obj_t *label_alt_unit;
static lv_obj_t *label_steps_val;
static lv_obj_t *label_steps_unit;
static lv_obj_t *icon_hr;
static lv_obj_t *icon_alt;
static lv_obj_t *icon_steps;

static lv_timer_t *refresh_timer;

extern const lv_font_t rubik_bold_68_digits;
extern const lv_image_dsc_t huwai_16;
extern const lv_image_dsc_t aixin_16;
extern const lv_image_dsc_t aixin_line_32;
extern const lv_image_dsc_t bushu_16;

static const char *months[] = {
    "JAN","FEB","MAR","APR","MAY","JUN",
    "JUL","AUG","SEP","OCT","NOV","DEC"
};
static const char *wdays[] = {
    "SUN","MON","TUE","WED","THU","FRI","SAT"
};

/* ── 刷新回调 ── */
static void on_refresh(lv_timer_t *t)
{
    (void)t;
    watch_time_t wt;
    watch_data_get_time(&wt);

    lv_label_set_text_fmt(label_time, "%02d#FF6600 :#%02d", wt.hour, wt.min);

    lv_label_set_text_fmt(label_date_top, "%s %02d", months[wt.month - 1], wt.day);
    lv_label_set_text(label_date_bot, wdays[wt.weekday]);

    int32_t bat = watch_data_get_battery();
    lv_label_set_text_fmt(label_status_num, "%d", (int)bat);
    lv_arc_set_value(status_arc, (int16_t)bat);

    int32_t hr = watch_data_get_heart_rate();
    if (hr > 0) {
        lv_label_set_text_fmt(label_hr_val, "%d", (int)hr);
        lv_arc_set_value(ring_hr, (int16_t)((hr - 40) * 100 / 120));
    }

    int32_t alt = watch_data_get_altitude();
    int32_t a = alt < 0 ? -alt : alt;
    if (a < 1000)
        lv_label_set_text_fmt(label_alt_val, "%d", (int)alt);
    else
        lv_label_set_text_fmt(label_alt_val, "%s%d.%dk",
                              alt < 0 ? "-" : "",
                              (int)(a / 1000), (int)((a % 1000) / 100));
    int32_t alt_pct = a * 100 / 5000;
    if (alt_pct > 100) alt_pct = 100;
    lv_arc_set_value(ring_alt, (int16_t)alt_pct);

    int32_t steps = watch_data_get_steps();
    if (steps < 1000)
        lv_label_set_text_fmt(label_steps_val, "%d", (int)steps);
    else if (steps < 10000)
        lv_label_set_text_fmt(label_steps_val, "%d.%dk",
                              (int)(steps / 1000), (int)((steps % 1000) / 100));
    else
        lv_label_set_text_fmt(label_steps_val, "%dk", (int)(steps / 1000));
    int32_t st_pct = steps * 100 / 10000;
    if (st_pct > 100) st_pct = 100;
    lv_arc_set_value(ring_steps, (int16_t)st_pct);
}

/* ── 底部数据环小部件 ──
   容器 62×56，内含：56px 弧 + 18px 图标 + 数值 + 单位  */
static void make_ring_widget(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                             lv_color_t color,
                             const lv_image_dsc_t *icon_src,
                             lv_obj_t **arc_out, lv_obj_t **val_out,
                             lv_obj_t **unit_out, lv_obj_t **icon_out,
                             const char *unit_text)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 75, 78);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);

    /* 弧形 73×73 + 线宽5，不可交互 */
    lv_obj_t *arc = lv_arc_create(cont);
    lv_obj_set_size(arc, 73, 73);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 225, 495);  /* 底部留90°缺口给单位标签 */
    lv_arc_set_rotation(arc, 270);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc, 0, 0);
    lv_obj_set_style_pad_all(arc, 0, 0);

    /* 图标 16×16 原生尺寸 */
    lv_obj_t *icon = lv_image_create(cont);
    lv_image_set_src(icon, icon_src);
    lv_obj_set_size(icon, 16, 16);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -14);

    /* 数值 */
    lv_obj_t *val = lv_label_create(cont);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(val, lv_color_white(), 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_MID, 0, -18);

    /* 单位 */
    lv_obj_t *unit = lv_label_create(cont);
    lv_label_set_text(unit, unit_text);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(unit, color, 0);
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -2);

    *arc_out  = arc;
    *val_out  = val;
    *unit_out = unit;
    *icon_out = icon;
}

/* ═══════════════════════════════════════════════════════════════
   布局（240×280 屏幕，center=120,140 为 LV_ALIGN_CENTER 基准）

   y=4..48   电量环 (44×44)              日期 + 电量 (右上)
   y=54..154 时间 Rubik 100 (center y=104, offset -36)
   y=150..200 健康卡 186×50 (center y=175, offset +35)
   y=202..280 三个数据环 75×78
   ═══════════════════════════════════════════════════════════════ */

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_clip_corner(root, true, 0);

    /* ── 左上：电量环 44×44 (与右侧日期居中对齐) ── */
    status_arc = lv_arc_create(root);
    lv_obj_set_size(status_arc, 44, 44);
    lv_obj_set_pos(status_arc, 14, 0);
    lv_arc_set_mode(status_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(status_arc, 0, 100);
    lv_arc_set_value(status_arc, 0);
    lv_arc_set_bg_angles(status_arc, 0, 360);
    lv_arc_set_rotation(status_arc, 270);
    lv_obj_remove_flag(status_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_style(status_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(status_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(status_arc, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(status_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(status_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(status_arc, lv_color_hex(0x3EE8B6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(status_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(status_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(status_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_arc, 0, 0);
    lv_obj_set_style_pad_all(status_arc, 0, 0);

    label_status_num = lv_label_create(root);
    lv_label_set_text(label_status_num, "87");
    lv_obj_set_style_text_font(label_status_num, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_status_num, lv_color_white(), 0);
    lv_obj_align_to(label_status_num, status_arc, LV_ALIGN_CENTER, 0, 0);

    /* ── 右上：日期 (月份+日期 / 星期) ── */
    label_date_top = lv_label_create(root);
    lv_label_set_text(label_date_top, "DEC 31");  /* 最长测试 */
    lv_obj_set_style_text_font(label_date_top, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_date_top, lv_color_white(), 0);
        lv_obj_align(label_date_top, LV_ALIGN_TOP_RIGHT, -24, 2);

    label_date_bot = lv_label_create(root);
    lv_label_set_text(label_date_bot, "WED");
    lv_obj_set_style_text_font(label_date_bot, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_date_bot, lv_color_hex(0xFF6600), 0);
        lv_obj_align(label_date_bot, LV_ALIGN_TOP_RIGHT, -24, 26);

    /* ── 中间：时间 (center y=104, offset -36) ── */
    label_time = lv_label_create(root);
    lv_label_set_text(label_time, "22#FF6600 :#05");
    lv_obj_set_style_text_font(label_time, &rubik_bold_68_digits, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
        lv_label_set_recolor(label_time, true);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, -36);

    /* ── 健康摘要卡片 186×50 (center y=191, offset +51) ── */
    health_card = lv_obj_create(root);
    lv_obj_set_size(health_card, 186, 50);
    lv_obj_remove_flag(health_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(health_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(health_card, 0, 0);
    lv_obj_set_style_border_width(health_card, 1, 0);
    lv_obj_set_style_border_color(health_card, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_opa(health_card, LV_OPA_60, 0);
    lv_obj_set_style_radius(health_card, 18, 0);
    lv_obj_set_style_bg_opa(health_card, LV_OPA_TRANSP, 0);
    lv_obj_align(health_card, LV_ALIGN_CENTER, 0, 35);

    health_icon = lv_image_create(health_card);
    lv_image_set_src(health_icon, &aixin_line_32);
    lv_obj_set_size(health_icon, 26, 26);
    lv_obj_set_style_image_recolor(health_icon, lv_color_hex(0x3EE8B6), 0);
    lv_obj_set_style_image_recolor_opa(health_icon, LV_OPA_COVER, 0);
    lv_obj_align(health_icon, LV_ALIGN_LEFT_MID, 14, 0);

    health_title = lv_label_create(health_card);
    lv_label_set_text(health_title, "Body Stable");
    lv_obj_set_style_text_font(health_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(health_title, lv_color_white(), 0);
        lv_obj_align_to(health_title, health_icon, LV_ALIGN_OUT_RIGHT_TOP, 10, -2);

    health_subtitle = lv_label_create(health_card);
    lv_label_set_text(health_subtitle, "Heart Normal");
    lv_obj_set_style_text_font(health_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(health_subtitle, lv_color_hex(0x999999), 0);
        lv_obj_align_to(health_subtitle, health_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    /* ── 底部三个数据环 (y=202) ──
       3×75 = 225px, 边距2+3, 间距5+5 = 240px  */
    make_ring_widget(root, 2,   202, lv_color_hex(0xFF5A7A),
                     &aixin_16,
                     &ring_hr, &label_hr_val, &label_hr_unit, &icon_hr, "BPM");

    make_ring_widget(root, 82,  202, lv_color_hex(0x8F6AE1),
                     &huwai_16,
                     &ring_alt, &label_alt_val, &label_alt_unit, &icon_alt, "m");

    make_ring_widget(root, 162, 202, lv_color_hex(0x2DFF9D),
                     &bushu_16,
                     &ring_steps, &label_steps_val, &label_steps_unit, &icon_steps, "steps");

    /* 初始加载 */
    on_refresh(NULL);
    refresh_timer = lv_timer_create(on_refresh, 3000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}
static void update(void) {}

const page_t page_watchface = {
    .name    = "watchface",
    .type    = PAGE_TYPE_HUB,
    .create  = create,
    .destroy = destroy,
    .update  = update,
};
