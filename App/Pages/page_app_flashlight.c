#include "page.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *label;
static bool is_on;

static void click_cb(lv_event_t *e)
{
    if (gesture_was_recent()) return;
    is_on = !is_on;
    if (is_on) {
        lv_label_set_text(label, "ON");
        lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_set_style_bg_color(root, lv_color_white(), 0);
    } else {
        lv_label_set_text(label, "OFF");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    }
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    label = lv_label_create(root);
    lv_label_set_text(label, "OFF");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

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
