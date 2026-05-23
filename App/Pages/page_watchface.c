#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_time;
static lv_obj_t *label_ampm;
static lv_obj_t *label_date;
static lv_obj_t *card_widgets;
static lv_obj_t *label_hr_num, *label_hr_unit;
static lv_obj_t *label_spo2_num, *label_spo2_unit;
static lv_obj_t *label_steps_num, *label_steps_unit;
static lv_timer_t *refresh_timer;

extern const lv_image_dsc_t Pure_black_background;
extern const lv_image_dsc_t little_heart;
extern const lv_image_dsc_t spO2;
extern const lv_image_dsc_t little_lootprints;
extern const lv_font_t montserrat_48_digits;

static const char *month_names[] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December"
};
static const char *weekday_names[] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

static void on_refresh(lv_timer_t *timer)
{
    watch_time_t t;
    watch_data_get_time(&t);

    lv_label_set_text_fmt(label_time, "%02d:%02d", t.hour, t.min);
    lv_label_set_text(label_ampm, t.hour >= 12 ? "pm" : "am");

    lv_label_set_text_fmt(label_date, "%s %d,  #00D4AA %s#",
        month_names[t.month - 1], t.day, weekday_names[t.weekday]);

    /* 心率 */
    int hr = (int)watch_data_get_heart_rate();
    lv_label_set_text_fmt(label_hr_num, "%d", hr > 0 ? hr : 78);
    lv_label_set_text(label_hr_unit, "bpm");

    /* 血氧 */
    int spo2 = (int)watch_data_get_spo2();
    lv_label_set_text_fmt(label_spo2_num, "%d", spo2 > 0 ? spo2 : 98);
    lv_label_set_text(label_spo2_unit, "%");

    /* 步数 */
    int steps = (int)watch_data_get_steps();
    lv_label_set_text_fmt(label_steps_num, "%d", steps);
    lv_label_set_text(label_steps_unit, "Steps");
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(root, true, 0);

    /* ── 背景壁纸 ── */
    lv_obj_t *wallpaper = lv_image_create(root);
    lv_image_set_src(wallpaper, &Pure_black_background);
    lv_obj_set_size(wallpaper, 240, 280);
    lv_obj_align(wallpaper, LV_ALIGN_CENTER, 0, 0);

    /* ── 时间: "01:48" ── */
    label_time = lv_label_create(root);
    lv_label_set_text(label_time, "01:48");
    lv_obj_set_style_text_font(label_time, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 36);

    /* "pm" 小写灰色 */
    label_ampm = lv_label_create(root);
    lv_label_set_text(label_ampm, "pm");
    lv_obj_set_style_text_font(label_ampm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_ampm, lv_color_hex(0x888888), 0);
    lv_obj_align_to(label_ampm, label_time, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    /* ── 日期: "June 21, Monday" ── */
    label_date = lv_label_create(root);
    lv_label_set_recolor(label_date, true);
    lv_label_set_text(label_date, "June 21,  #00D4AA Monday#");
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_date, lv_color_white(), 0);
    lv_obj_align(label_date, LV_ALIGN_TOP_MID, 0, 100);

    /* ── 底部卡片: 3 槽位 widget 容器 ── */
    card_widgets = lv_obj_create(root);
    lv_obj_set_size(card_widgets, 216, 118);
    lv_obj_set_style_bg_color(card_widgets, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card_widgets, LV_OPA_10, 0);
    lv_obj_set_style_border_width(card_widgets, 0, 0);
    lv_obj_set_style_radius(card_widgets, 18, 0);
    lv_obj_set_style_pad_all(card_widgets, 0, 0);
    lv_obj_align(card_widgets, LV_ALIGN_BOTTOM_MID, 0, -12);

    /* ── 卡片内: 槽位1 — 心率 ── */
    lv_obj_t *icon_hr = lv_image_create(card_widgets);
    lv_image_set_src(icon_hr, &little_heart);
    lv_image_set_scale(icon_hr, 384);
    lv_obj_align(icon_hr, LV_ALIGN_TOP_LEFT, 28, 13);

    label_hr_num = lv_label_create(card_widgets);
    lv_label_set_text(label_hr_num, "78");
    lv_obj_set_style_text_font(label_hr_num, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_hr_num, lv_color_hex(0xFFD700), 0);
    lv_obj_align(label_hr_num, LV_ALIGN_TOP_LEFT, 72, 8);

    label_hr_unit = lv_label_create(card_widgets);
    lv_label_set_text(label_hr_unit, "bpm");
    lv_obj_set_style_text_font(label_hr_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_hr_unit, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_hr_unit, label_hr_num, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* ── 卡片内: 槽位2 — 血氧 ── */
    lv_obj_t *icon_spo2 = lv_image_create(card_widgets);
    lv_image_set_src(icon_spo2, &spO2);
    lv_image_set_scale(icon_spo2, 320);
    lv_obj_align(icon_spo2, LV_ALIGN_TOP_LEFT, 25, 49);

    label_spo2_num = lv_label_create(card_widgets);
    lv_label_set_text(label_spo2_num, "98");
    lv_obj_set_style_text_font(label_spo2_num, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_spo2_num, lv_color_hex(0xFFD700), 0);
    lv_obj_align(label_spo2_num, LV_ALIGN_TOP_LEFT, 72, 46);

    label_spo2_unit = lv_label_create(card_widgets);
    lv_label_set_text(label_spo2_unit, "%");
    lv_obj_set_style_text_font(label_spo2_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_spo2_unit, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_spo2_unit, label_spo2_num, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    /* ── 卡片内: 槽位3 — 步数 ── */
    lv_obj_t *icon_steps = lv_image_create(card_widgets);
    lv_image_set_src(icon_steps, &little_lootprints);
    lv_obj_align(icon_steps, LV_ALIGN_TOP_LEFT, 28, 86);

    label_steps_num = lv_label_create(card_widgets);
    lv_label_set_text(label_steps_num, "7,299");
    lv_obj_set_style_text_font(label_steps_num, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_steps_num, lv_color_hex(0xFFD700), 0);
    lv_obj_align(label_steps_num, LV_ALIGN_TOP_LEFT, 72, 83);

    label_steps_unit = lv_label_create(card_widgets);
    lv_label_set_text(label_steps_unit, "Steps");
    lv_obj_set_style_text_font(label_steps_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_steps_unit, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_steps_unit, label_steps_num, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

static void update(void) {}

const page_t page_watchface = {
    .name = "watchface",
    .type = PAGE_TYPE_HUB,
    .create = create,
    .destroy = destroy,
    .update = update,
};
