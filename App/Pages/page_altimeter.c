#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_altitude;
static lv_obj_t *label_pressure;
static lv_timer_t *refresh_timer;

extern const lv_image_dsc_t elevation;
extern const lv_image_dsc_t barometric_pressure;
extern const lv_font_t montserrat_48_digits;

static void on_refresh(lv_timer_t *timer)
{
    int32_t alt = watch_data_get_altitude();
    int32_t press = watch_data_get_pressure();

    lv_label_set_text_fmt(label_altitude, "%d", alt);
    lv_label_set_text_fmt(label_pressure, "%d", press);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ==================== 中心分割线 ==================== */
    lv_obj_t *divider = lv_obj_create(root);
    lv_obj_set_size(divider, 200, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 0);

    /* ==================== 上半区：海拔 ==================== */

    /* 海拔图标 */
    lv_obj_t *alt_icon = lv_image_create(root);
    lv_image_set_src(alt_icon, &elevation);
    lv_obj_align(alt_icon, LV_ALIGN_CENTER, -84, -70);

    /* 海拔数值 — 亮橙色 */
    label_altitude = lv_label_create(root);
    lv_label_set_text(label_altitude, "156");
    lv_obj_set_style_text_font(label_altitude, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_altitude, lv_color_hex(0xFF6600), 0);
    lv_obj_align(label_altitude, LV_ALIGN_CENTER, 30, -84);

    /* 单位 Altitude (m) */
    lv_obj_t *label_alt_unit = lv_label_create(root);
    lv_label_set_text(label_alt_unit, "Altitude (m)");
    lv_obj_set_style_text_font(label_alt_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_alt_unit, lv_color_hex(0x808080), 0);
    lv_obj_align(label_alt_unit, LV_ALIGN_CENTER, 30, -40);

    /* ==================== 下半区：气压 ==================== */

    /* 气压图标 */
    lv_obj_t *baro_icon = lv_image_create(root);
    lv_image_set_src(baro_icon, &barometric_pressure);
    lv_obj_align(baro_icon, LV_ALIGN_CENTER, -84, 60);

    /* 气压数值 */
    label_pressure = lv_label_create(root);
    lv_label_set_text(label_pressure, "1013");
    lv_obj_set_style_text_font(label_pressure, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_pressure, lv_color_white(), 0);
    lv_obj_align(label_pressure, LV_ALIGN_CENTER, 30, 46);

    /* 单位 Baro (hPa) */
    lv_obj_t *label_baro_unit = lv_label_create(root);
    lv_label_set_text(label_baro_unit, "Baro (hPa)");
    lv_obj_set_style_text_font(label_baro_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_baro_unit, lv_color_hex(0x808080), 0);
    lv_obj_align(label_baro_unit, LV_ALIGN_CENTER, 30, 90);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

static void update(void) {}

const page_t page_altimeter = {
    .name = "altimeter",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
