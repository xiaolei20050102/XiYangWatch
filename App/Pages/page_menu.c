#include "page.h"

static lv_obj_t *root;

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* TODO: 12 项菜单列表 */

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "Menu\n\nTODO");
    lv_obj_center(label);

    return root;
}

static void destroy(void) {}
static void update(void) {}

const page_t page_menu = {
    .name = "menu",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
