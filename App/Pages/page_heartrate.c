#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_timer_t *refresh_timer;

static void on_refresh(lv_timer_t *timer)
{
    /* TODO: 更新心率数字和区间指示 */
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* TODO: 心率大数字 + bpm 标签 + 区间指示条 + 静息心率 */

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

static void update(void) {}

const page_t page_heartrate = {
    .name = "heartrate",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
