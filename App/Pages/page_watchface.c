#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_hour_shadow;
static lv_obj_t *label_hour;
static lv_obj_t *label_min_shadow;
static lv_obj_t *label_min;
static lv_obj_t *label_ampm_shadow;
static lv_obj_t *label_ampm;
static lv_obj_t *label_date_shadow;
static lv_obj_t *label_date;
static lv_timer_t *refresh_timer;

extern const lv_font_t rubik_bold_100_digits;

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

    lv_label_set_text_fmt(label_hour_shadow, "%02d", t.hour);
    lv_label_set_text_fmt(label_hour, "%02d", t.hour);
    lv_label_set_text_fmt(label_min_shadow, "%02d", t.min);
    lv_label_set_text_fmt(label_min, "%02d", t.min);

    const char *ampm = t.hour >= 12 ? "Pm" : "Am";
    lv_label_set_text(label_ampm_shadow, ampm);
    lv_label_set_text(label_ampm, ampm);

    lv_label_set_text_fmt(label_date_shadow, "#000000 %s %d, # #000000 %s#",
        month_names[t.month - 1], t.day, weekday_names[t.weekday]);
    lv_label_set_text_fmt(label_date, "#FFFFFF %s %d, # #FF5500 %s#",
        month_names[t.month - 1], t.day, weekday_names[t.weekday]);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_clip_corner(root, true, 0);

    /* 小时阴影层 */
    label_hour_shadow = lv_label_create(root);
    lv_obj_set_style_text_font(label_hour_shadow, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_hour_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_opa(label_hour_shadow, LV_OPA_50, 0);
    lv_obj_align(label_hour_shadow, LV_ALIGN_CENTER, 2, -54);

    /* 小时前景层 */
    label_hour = lv_label_create(root);
    lv_obj_set_style_text_font(label_hour, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_hour, LV_ALIGN_CENTER, 0, -56);

    /* 分钟阴影层 */
    label_min_shadow = lv_label_create(root);
    lv_obj_set_style_text_font(label_min_shadow, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_min_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_opa(label_min_shadow, LV_OPA_40, 0);
    lv_obj_align(label_min_shadow, LV_ALIGN_CENTER, 2, 38);

    /* 分钟前景层 */
    label_min = lv_label_create(root);
    lv_obj_set_style_text_font(label_min, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_min, lv_color_hex(0xFF5500), 0);
    lv_obj_align(label_min, LV_ALIGN_CENTER, 0, 36);

    /* Am/Pm 阴影层 */
    label_ampm_shadow = lv_label_create(root);
    lv_obj_set_style_text_font(label_ampm_shadow, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_ampm_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_opa(label_ampm_shadow, LV_OPA_50, 0);
    lv_obj_align_to(label_ampm_shadow, label_min, LV_ALIGN_OUT_RIGHT_MID, 9, -45);

    /* Am/Pm 前景层 */
    label_ampm = lv_label_create(root);
    lv_obj_set_style_text_font(label_ampm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_ampm, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_ampm, label_min, LV_ALIGN_OUT_RIGHT_MID, 8, -46);

    /* 日期阴影层 */
    label_date_shadow = lv_label_create(root);
    lv_label_set_recolor(label_date_shadow, true);
    lv_obj_set_style_text_font(label_date_shadow, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_date_shadow, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_opa(label_date_shadow, LV_OPA_50, 0);
    lv_obj_align(label_date_shadow, LV_ALIGN_BOTTOM_MID, 1, -37);

    /* 日期前景层 */
    label_date = lv_label_create(root);
    lv_label_set_recolor(label_date, true);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0x606060), 0);
    lv_obj_align(label_date, LV_ALIGN_BOTTOM_MID, 0, -38);

    /* load initial data from provider */
    on_refresh(NULL);
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
