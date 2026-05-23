#include "page.h"

static lv_obj_t *root;

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* TODO: 固件版本 + 存储信息 */

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "About\n\nXiYangWatch v1.0\nSTM32F411 + LVGL 9.5");
    lv_obj_center(label);

    return root;
}

static void destroy(void) {}
static void update(void) {}

const page_t page_about = {
    .name = "about",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
