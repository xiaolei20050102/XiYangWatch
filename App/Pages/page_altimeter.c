#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_altitude;
static lv_obj_t *label_pressure;
static lv_obj_t *bar_altitude;
static lv_obj_t *bar_pressure;
static lv_obj_t *icon_alt;
static lv_obj_t *icon_baro;
static lv_obj_t *container_alt;
static lv_obj_t *container_baro;
static lv_timer_t *refresh_timer;

extern const lv_image_dsc_t elevation;
extern const lv_image_dsc_t barometric_pressure;
extern const lv_font_t montserrat_48_digits;

static const lv_point_precise_t div_pts[] = { {20, 0}, {220, 0} };

static void translate_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void bar_anim_cb(void *var, int32_t v)
{
    lv_bar_set_value((lv_obj_t *)var, v, LV_ANIM_OFF);
}

static void on_refresh(lv_timer_t *timer)
{
    int32_t alt = watch_data_get_altitude();
    int32_t press = watch_data_get_pressure();
    lv_label_set_text_fmt(label_altitude, "%d", alt);
    lv_label_set_text_fmt(label_pressure, "%d", press);
    lv_bar_set_value(bar_altitude, alt, LV_ANIM_OFF);
    lv_bar_set_value(bar_pressure, press, LV_ANIM_OFF);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ── 中心分割线 (极暗细线) ── */
    lv_obj_t *divider = lv_line_create(root);
    lv_line_set_points(divider, div_pts, 2);
    lv_obj_set_style_line_color(divider, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_line_width(divider, 1, 0);
    lv_obj_set_style_line_opa(divider, LV_OPA_COVER, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 140);

    /* ==================== 上半区：海拔 ==================== */

    /* 山峰图标 */
    icon_alt = lv_image_create(root);
    lv_image_set_src(icon_alt, &elevation);
    lv_obj_set_style_image_recolor(icon_alt, lv_color_hex(0x0055AA), 0);
    lv_obj_set_style_image_recolor_opa(icon_alt, LV_OPA_COVER, 0);
    lv_obj_align(icon_alt, LV_ALIGN_TOP_LEFT, 20, 40);

    /* 数据容器 (数字 + 说明 + 光剑条) */
    container_alt = lv_obj_create(root);
    lv_obj_set_size(container_alt, 140, 80);
    lv_obj_set_style_pad_all(container_alt, 0, 0);
    lv_obj_set_style_border_width(container_alt, 0, 0);
    lv_obj_set_style_bg_opa(container_alt, LV_OPA_TRANSP, 0);
    lv_obj_align(container_alt, LV_ALIGN_TOP_LEFT, 80, 30);

    label_altitude = lv_label_create(container_alt);
    lv_label_set_text(label_altitude, "156");
    lv_obj_set_style_text_font(label_altitude, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_altitude, lv_color_hex(0xFF6600), 0);
    lv_obj_align(label_altitude, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *label_alt_unit = lv_label_create(container_alt);
    lv_label_set_text(label_alt_unit, "Altitude (m)");
    lv_obj_set_style_text_font(label_alt_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_alt_unit, lv_color_hex(0x777777), 0);
    lv_obj_align_to(label_alt_unit, label_altitude, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

    bar_altitude = lv_bar_create(container_alt);
    lv_obj_set_size(bar_altitude, 140, 2);
    lv_bar_set_range(bar_altitude, -500, 10000);
    lv_bar_set_value(bar_altitude, -500, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar_altitude, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_altitude, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_altitude, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_altitude, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_altitude, lv_color_hex(0x0055AA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_altitude, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar_altitude, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_altitude, 0, 0);
    lv_obj_align_to(bar_altitude, label_alt_unit, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    /* ==================== 下半区：气压 ==================== */

    /* 气压图标 */
    icon_baro = lv_image_create(root);
    lv_image_set_src(icon_baro, &barometric_pressure);
    lv_obj_set_style_image_recolor(icon_baro, lv_color_hex(0xE60000), 0);
    lv_obj_set_style_image_recolor_opa(icon_baro, LV_OPA_COVER, 0);
    lv_obj_align(icon_baro, LV_ALIGN_TOP_LEFT, 20, 180);

    /* 数据容器 */
    container_baro = lv_obj_create(root);
    lv_obj_set_size(container_baro, 140, 80);
    lv_obj_set_style_pad_all(container_baro, 0, 0);
    lv_obj_set_style_border_width(container_baro, 0, 0);
    lv_obj_set_style_bg_opa(container_baro, LV_OPA_TRANSP, 0);
    lv_obj_align(container_baro, LV_ALIGN_TOP_LEFT, 80, 170);

    label_pressure = lv_label_create(container_baro);
    lv_label_set_text(label_pressure, "1013");
    lv_obj_set_style_text_font(label_pressure, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_pressure, lv_color_white(), 0);
    lv_obj_align(label_pressure, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *label_baro_unit = lv_label_create(container_baro);
    lv_label_set_text(label_baro_unit, "Baro (hPa)");
    lv_obj_set_style_text_font(label_baro_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_baro_unit, lv_color_hex(0x777777), 0);
    lv_obj_align_to(label_baro_unit, label_pressure, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

    bar_pressure = lv_bar_create(container_baro);
    lv_obj_set_size(bar_pressure, 140, 2);
    lv_bar_set_range(bar_pressure, 800, 1100);
    lv_bar_set_value(bar_pressure, 800, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar_pressure, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_pressure, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_pressure, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_pressure, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar_pressure, lv_color_hex(0xE60000), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_pressure, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar_pressure, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_pressure, 0, 0);
    lv_obj_align_to(bar_pressure, label_baro_unit, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    /* ── 入场动画 ── */

    /* ① 图标从左侧滑入 (-40 → 20) */
    lv_obj_set_style_translate_x(icon_alt, -60, 0);
    lv_obj_set_style_translate_x(icon_baro, -60, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, icon_alt);
    lv_anim_set_values(&a, -60, 0);
    lv_anim_set_exec_cb(&a, translate_anim_cb);
    lv_anim_set_duration(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);

    lv_anim_set_var(&a, icon_baro);
    lv_anim_set_delay(&a, 80);
    lv_anim_start(&a);

    /* ② 数据组从右侧滑入 (240 → 80) */
    lv_obj_set_style_translate_x(container_alt, 160, 0);
    lv_obj_set_style_translate_x(container_baro, 160, 0);

    lv_anim_set_var(&a, container_alt);
    lv_anim_set_values(&a, 160, 0);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_duration(&a, 450);
    lv_anim_start(&a);

    lv_anim_set_var(&a, container_baro);
    lv_anim_set_delay(&a, 180);
    lv_anim_start(&a);

    /* ③ 光剑充能 (bar 值从 min 充到当前值) */
    int32_t cur_alt = watch_data_get_altitude();
    int32_t cur_press = watch_data_get_pressure();
    if (cur_alt < -500) cur_alt = -500;
    if (cur_alt > 10000) cur_alt = 10000;

    lv_anim_set_var(&a, bar_altitude);
    lv_anim_set_values(&a, -500, cur_alt);
    lv_anim_set_exec_cb(&a, bar_anim_cb);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_delay(&a, 300);
    lv_anim_start(&a);

    lv_anim_set_var(&a, bar_pressure);
    lv_anim_set_values(&a, 800, cur_press);
    lv_anim_set_delay(&a, 400);
    lv_anim_start(&a);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    lv_anim_delete(NULL, translate_anim_cb);
    lv_anim_delete(NULL, bar_anim_cb);
}

static void update(void) {}

const page_t page_altimeter = {
    .name = "altimeter",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
