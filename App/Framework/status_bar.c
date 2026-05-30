#include "status_bar.h"
#include "../Data/data_provider.h"

static lv_obj_t *bar;
static lv_obj_t *label_time;
static lv_obj_t *label_battery;
static lv_obj_t *arc_battery;

void status_bar_create(lv_obj_t *parent)
{
    bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 240, 20);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 14, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    /* center: time */
    label_time = lv_label_create(bar);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);

    /* right: battery percent */
    label_battery = lv_label_create(bar);
    lv_obj_set_style_text_font(label_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_battery, lv_color_white(), 0);
    lv_obj_align(label_battery, LV_ALIGN_RIGHT_MID, -36, 0);

    /* right: battery ring, placed directly on bar (no flex wrapper) */
    arc_battery = lv_arc_create(bar);
    lv_obj_remove_style_all(arc_battery);
    lv_obj_set_size(arc_battery, 14, 14);
    lv_obj_set_style_pad_all(arc_battery, 0, 0);
    lv_obj_set_style_bg_opa(arc_battery, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc_battery, 0, 0);
    lv_obj_set_style_arc_width(arc_battery, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_battery, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_battery, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_battery, lv_color_hex(0x3EE8B6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_battery, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_battery, false, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc_battery, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_arc_set_range(arc_battery, 0, 100);
    lv_arc_set_bg_angles(arc_battery, 0, 360);
    lv_arc_set_rotation(arc_battery, 270);
    lv_obj_align(arc_battery, LV_ALIGN_RIGHT_MID, -12, 0);

    status_bar_refresh_time();
    status_bar_refresh_battery();
}

void status_bar_bring_to_front(void)
{
    if (bar) lv_obj_move_foreground(bar);
}

void status_bar_set_visible(bool visible)
{
    if (!bar) return;
    if (visible) {
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
}

void status_bar_refresh_time(void)
{
    if (!label_time) return;
    if (lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN)) return;
    watch_time_t t;
    watch_data_get_time(&t);
    lv_label_set_text_fmt(label_time, "%02d:%02d", t.hour, t.min);
}

void status_bar_refresh_battery(void)
{
    if (!bar) return;
    if (lv_obj_has_flag(bar, LV_OBJ_FLAG_HIDDEN)) return;
    int32_t pct = watch_data_get_battery();
    if (label_battery) lv_label_set_text_fmt(label_battery, "%d%%", (int)pct);
    if (arc_battery) lv_arc_set_value(arc_battery, pct);
}
