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

    /* TODO: 表盘实时预览 + 左右滑切换样式 */

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "Watchface\nSelector\n\nTODO");
    lv_obj_center(label);

    return root;
}

static void destroy(void) {}
static void update(void) {}

const page_t page_watchface_sel = {
    .name = "watchface_sel",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
