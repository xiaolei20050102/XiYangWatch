#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *card_temp;
static lv_obj_t *card_humi;
static lv_obj_t *label_temp;
static lv_obj_t *label_humi;
static lv_timer_t *refresh_timer;

extern const lv_image_dsc_t new_temperature_32;
extern const lv_image_dsc_t humidity_32;
extern const lv_font_t montserrat_48_digits;

static void card_translate_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void on_refresh(lv_timer_t *timer)
{
    int32_t temp = watch_data_get_temperature();
    int32_t humi = watch_data_get_humidity();
    lv_label_set_text_fmt(label_temp, "%d", temp / 10);
    lv_label_set_text_fmt(label_humi, "%d", humi);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ==================== 温度卡片 (上方) ==================== */
    card_temp = lv_obj_create(root);
    lv_obj_set_size(card_temp, 220, 120);
    lv_obj_set_style_bg_color(card_temp, lv_color_hex(0x2A1100), 0);
    lv_obj_set_style_bg_opa(card_temp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card_temp, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_border_width(card_temp, 1, 0);
    lv_obj_set_style_border_opa(card_temp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card_temp, 20, 0);
    lv_obj_set_style_pad_all(card_temp, 0, 0);
    lv_obj_align(card_temp, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *temp_icon = lv_image_create(card_temp);
    lv_image_set_src(temp_icon, &new_temperature_32);
    lv_obj_align(temp_icon, LV_ALIGN_TOP_LEFT, 12, 12);

    lv_obj_t *temp_label = lv_label_create(card_temp);
    lv_label_set_text(temp_label, "TEMP");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xA34700), 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 12, 48);

    label_temp = lv_label_create(card_temp);
    lv_label_set_text(label_temp, "22");
    lv_obj_set_style_text_font(label_temp, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_temp, lv_color_white(), 0);
    lv_obj_align(label_temp, LV_ALIGN_BOTTOM_RIGHT, -42, -8);

    lv_obj_t *temp_unit = lv_label_create(card_temp);
    lv_label_set_text(temp_unit, "\302\260C");
    lv_obj_set_style_text_font(temp_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(temp_unit, lv_color_hex(0x808080), 0);
    lv_obj_align_to(temp_unit, label_temp, LV_ALIGN_OUT_RIGHT_TOP, 4, 4);

    /* ==================== 湿度卡片 (下方) ==================== */
    card_humi = lv_obj_create(root);
    lv_obj_set_size(card_humi, 220, 120);
    lv_obj_set_style_bg_color(card_humi, lv_color_hex(0x001A26), 0);
    lv_obj_set_style_bg_opa(card_humi, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card_humi, lv_color_hex(0x00D2FF), 0);
    lv_obj_set_style_border_width(card_humi, 1, 0);
    lv_obj_set_style_border_opa(card_humi, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card_humi, 20, 0);
    lv_obj_set_style_pad_all(card_humi, 0, 0);
    lv_obj_align(card_humi, LV_ALIGN_TOP_MID, 0, 145);

    lv_obj_t *humi_icon = lv_image_create(card_humi);
    lv_image_set_src(humi_icon, &humidity_32);
    lv_obj_align(humi_icon, LV_ALIGN_TOP_LEFT, 12, 12);

    lv_obj_t *humi_label = lv_label_create(card_humi);
    lv_label_set_text(humi_label, "HUMID");
    lv_obj_set_style_text_font(humi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(humi_label, lv_color_hex(0x006F87), 0);
    lv_obj_align(humi_label, LV_ALIGN_TOP_LEFT, 12, 48);

    label_humi = lv_label_create(card_humi);
    lv_label_set_text(label_humi, "55");
    lv_obj_set_style_text_font(label_humi, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_humi, lv_color_white(), 0);
    lv_obj_align(label_humi, LV_ALIGN_BOTTOM_RIGHT, -42, -8);

    lv_obj_t *humi_unit = lv_label_create(card_humi);
    lv_label_set_text(humi_unit, "%");
    lv_obj_set_style_text_font(humi_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(humi_unit, lv_color_hex(0x808080), 0);
    lv_obj_align_to(humi_unit, label_humi, LV_ALIGN_OUT_RIGHT_TOP, 4, 4);

    /* ── 入场动画：用 translate_x 做滑入，不破坏 LVGL 对齐 ── */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card_temp);
    lv_anim_set_values(&a, -250, 0);
    lv_anim_set_exec_cb(&a, card_translate_anim_cb);
    lv_anim_set_duration(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);

    lv_anim_set_var(&a, card_humi);
    lv_anim_set_values(&a, 250, 0);
    lv_anim_set_delay(&a, 80);
    lv_anim_start(&a);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    lv_anim_delete(NULL, card_translate_anim_cb);
}

static void update(void) {}

const page_t page_climate = {
    .name = "climate",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
