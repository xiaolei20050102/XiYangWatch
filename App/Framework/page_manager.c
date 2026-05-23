#include "page_manager.h"

#define OVERLAY_STACK_MAX 8

static page_state_t  g_state;
static page_id_t     g_overlay_stack[OVERLAY_STACK_MAX];
static int32_t       g_overlay_depth;
static lv_obj_t     *g_current_page;

/* 当前活跃页面索引 */
static page_id_t g_active_page;

void page_manager_init(void)
{
    g_state = STATE_AT_HUB;
    g_overlay_depth = 0;
    g_active_page = PAGE_WATCHFACE;

    const page_t *p = pages_config_get(PAGE_WATCHFACE);
    if (p && p->create) {
        g_current_page = p->create(lv_screen_active());
    }
}

void page_manager_go_home(void)
{
    /* TODO: destroy current, create watchface, reset state */
}

void page_manager_go_spoke(spoke_dir_t dir)
{
    /* TODO: map direction → page_id, switch to spoke */
}

void page_manager_push(page_id_t id)
{
    /* TODO: push overlay to stack, create new page */
}

void page_manager_pop(void)
{
    /* TODO: pop overlay, destroy current, create previous */
}

void page_manager_update(void)
{
    const page_t *p = pages_config_get(g_active_page);
    if (p && p->update) p->update();
}

page_state_t page_manager_get_state(void)
{
    return g_state;
}
