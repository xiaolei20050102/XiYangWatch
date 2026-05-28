#include "page.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *icon;
static bool is_on;

extern const lv_image_dsc_t Flashlight_Off;
extern const lv_image_dsc_t Flashlight_On;

static void click_cb(lv_event_t *e)
{
    if (gesture_was_recent()) return;
    is_on = !is_on;
    if (is_on) {
        lv_image_set_src(icon, &Flashlight_On);
        lv_obj_set_style_bg_color(root, lv_color_white(), 0);
    } else {
        lv_image_set_src(icon, &Flashlight_Off);
        lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    }
    lv_obj_move_foreground(icon);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    icon = lv_image_create(root);
    lv_image_set_src(icon, &Flashlight_Off);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, click_cb, LV_EVENT_CLICKED, NULL);

    is_on = false;
    return root;
}

static void destroy(void) {}

const page_t page_app_flashlight = {
    .name = "flashlight",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = NULL,
};
