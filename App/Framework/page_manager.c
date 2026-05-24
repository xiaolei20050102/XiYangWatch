#include "page_manager.h"
#include "lvgl.h"

#define OVERLAY_STACK_MAX 8

static page_state_t  g_state;
static page_id_t     g_overlay_stack[OVERLAY_STACK_MAX];
static int32_t       g_overlay_depth;
static lv_obj_t     *g_current_page;
static page_id_t     g_active_page;
static spoke_dir_t   g_entry_dir;
static int32_t       g_spoke_position;

/* ── crossfade state ── */
static bool          g_transitioning;
static const page_t *g_old_def;
static lv_obj_t     *g_old_obj;
static int32_t       g_fade_dummy;

/* ── spoke chain definitions ── */
static page_id_t chain_health[] = { PAGE_ACTIVITY, PAGE_HEARTRATE };
/* future: chain_env[] = { PAGE_ALTIMETER, PAGE_CLIMATE }; */

static page_id_t *page_manager_get_chain(spoke_dir_t dir, int32_t *count)
{
    switch (dir) {
    case SPOKE_RIGHT: *count = 2; return chain_health;
    default:          *count = 0; return NULL;
    }
}

/* ── crossfade helpers ── */
static void fade_cleanup(void)
{
    if (!g_old_obj) return;
    if (g_old_def && g_old_def->destroy) g_old_def->destroy();
    lv_obj_delete(g_old_obj);
    g_old_obj = NULL;
    g_old_def = NULL;
}

static void fade_in_cb(void *var, int32_t v)
{
    (void)var;
    if (g_current_page) lv_obj_set_style_opa(g_current_page, v, 0);
}

static void fade_in_done(lv_anim_t *a)
{
    (void)a;
    fade_cleanup();
    g_transitioning = false;
}

/* ── internal helper ── */
static void page_manager_switch_to(page_id_t id)
{
    if (g_transitioning) return;
    g_transitioning = true;

    /* force-cleanup any pending crossfade */
    fade_cleanup();

    const page_t *p = pages_config_get(id);
    if (!p || !p->create) { g_transitioning = false; return; }

    lv_obj_t *new_page = p->create(lv_screen_active());
    lv_obj_add_flag(new_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(new_page, LV_OPA_TRANSP, 0);

    if (g_current_page) {
        /* fade new page in over old page, then delete old */
        g_old_def = pages_config_get(g_active_page);
        g_old_obj = g_current_page;

        lv_obj_remove_flag(new_page, LV_OBJ_FLAG_HIDDEN);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, &g_fade_dummy);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, fade_in_cb);
        lv_anim_set_duration(&a, 150);
        lv_anim_set_completed_cb(&a, fade_in_done);
        lv_anim_start(&a);
    } else {
        g_transitioning = false;
    }

    g_current_page = new_page;
    g_active_page = id;
}

/* ── public API ── */
void page_manager_init(void)
{
    g_state = STATE_AT_HUB;
    g_overlay_depth = 0;
    g_active_page = PAGE_WATCHFACE;
    g_spoke_position = 0;

    /* 屏幕默认背景设为黑色，避免页面切换过渡时透出白底 */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    const page_t *p = pages_config_get(PAGE_WATCHFACE);
    if (p && p->create) {
        g_current_page = p->create(lv_screen_active());
    }
}

void page_manager_go_home(void)
{
    page_manager_switch_to(PAGE_WATCHFACE);
    g_state = STATE_AT_HUB;
    g_spoke_position = 0;
}

void page_manager_go_spoke(spoke_dir_t dir)
{
    if (g_state != STATE_AT_HUB) return;

    int32_t count;
    page_id_t *chain = page_manager_get_chain(dir, &count);
    if (!chain || count == 0) return;

    g_entry_dir = dir;
    g_spoke_position = 0;
    page_manager_switch_to(chain[0]);
    g_state = STATE_AT_SPOKE;
}

void page_manager_spoke_next(void)
{
    int32_t count;
    page_id_t *chain = page_manager_get_chain(g_entry_dir, &count);
    if (!chain || g_spoke_position + 1 >= count) return;

    g_spoke_position++;
    page_manager_switch_to(chain[g_spoke_position]);
}

void page_manager_spoke_prev(void)
{
    if (g_spoke_position > 0) {
        g_spoke_position--;
        int32_t count;
        page_id_t *chain = page_manager_get_chain(g_entry_dir, &count);
        page_manager_switch_to(chain[g_spoke_position]);
    } else {
        page_manager_go_home();
    }
}

void page_manager_push(page_id_t id)
{
    page_manager_switch_to(id);
    g_state = STATE_AT_OVERLAY;
}

void page_manager_pop(void)
{
    page_manager_go_home();
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

spoke_dir_t page_manager_get_entry_dir(void)
{
    return g_entry_dir;
}
