#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_timer_t *refresh_timer;

static void on_refresh(lv_timer_t *timer)
{
    /* TODO: 更新步数/卡路里/距离 */
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* TODO: 步数/卡路里/距离 + 本周柱状图 */

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "Activity\n\nTODO");
    lv_obj_center(label);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

static void update(void) {}

const page_t page_activity = {
    .name = "activity",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
