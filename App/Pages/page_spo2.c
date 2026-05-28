#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* TODO: Phase 2 — 测量按钮 + 结果 + 历史入口 */

    lv_obj_t *label_shadow = lv_label_create(root);
    lv_label_set_text(label_shadow, "SpO2\n\nPhase 2");
    lv_obj_set_style_text_color(label_shadow, lv_color_black(), 0);
    lv_obj_set_style_text_opa(label_shadow, LV_OPA_50, 0);
    lv_obj_align(label_shadow, LV_ALIGN_CENTER, 1, 1);

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "SpO2\n\nPhase 2");
    lv_obj_center(label);

    return root;
}

static void destroy(void) {}
static void update(void) {}

const page_t page_spo2 = {
    .name = "spo2",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
