#include "page.h"

static lv_obj_t *create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "Calculator\n\nPhase 2");
    lv_obj_center(label);
    return root;
}

static void destroy(void) {}
static void update(void) {}

const page_t page_calculator = {
    .name = "calculator",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
