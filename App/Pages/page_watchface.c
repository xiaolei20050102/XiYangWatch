#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_hour;
static lv_obj_t *label_min;
static lv_obj_t *label_ampm;
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

    lv_label_set_text_fmt(label_hour, "%02d", t.hour);
    lv_label_set_text_fmt(label_min, "%02d", t.min);
    lv_label_set_text(label_ampm, t.hour >= 12 ? "Pm" : "Am");

    lv_label_set_text_fmt(label_date, "#FFFFFF %s %d, # #FF5500 %s#",
        month_names[t.month - 1], t.day, weekday_names[t.weekday]);
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

    /* 小时 — 浅蓝 */
    label_hour = lv_label_create(root);
    lv_obj_set_style_text_font(label_hour, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_hour, LV_ALIGN_CENTER, 0, -56);

    /* 分钟 — 浅绿 */
    label_min = lv_label_create(root);
    lv_obj_set_style_text_font(label_min, &rubik_bold_100_digits, 0);
    lv_obj_set_style_text_color(label_min, lv_color_hex(0xFF5500), 0);
    lv_obj_align(label_min, LV_ALIGN_CENTER, 0, 36);

    /* Am — 右侧小灰字 */
    label_ampm = lv_label_create(root);
    lv_obj_set_style_text_font(label_ampm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_ampm, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_ampm, label_min, LV_ALIGN_OUT_RIGHT_MID, 8, -46);

    /* 日期 — 底部小灰字 */
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
