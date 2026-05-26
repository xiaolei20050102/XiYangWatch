#include "page_manager.h"
#include "lvgl.h"

#define OVERLAY_STACK_MAX 8

typedef enum {
    TRANS_FROM_RIGHT,
    TRANS_FROM_LEFT,
} trans_dir_t;

/* 手指左滑 → 新页从右进；手指右滑 → 新页从左进 */
static inline trans_dir_t trans_forward(spoke_dir_t spoke) {
    return (spoke == SPOKE_LEFT) ? TRANS_FROM_RIGHT : TRANS_FROM_LEFT;
}
static inline trans_dir_t trans_backward(spoke_dir_t spoke) {
    return (spoke == SPOKE_LEFT) ? TRANS_FROM_LEFT : TRANS_FROM_RIGHT;
}

static page_state_t  g_state;
static page_id_t     g_overlay_stack[OVERLAY_STACK_MAX];
static int32_t       g_overlay_depth;
static lv_obj_t     *g_current_page;
static page_id_t     g_active_page;
static spoke_dir_t   g_entry_dir;
static int32_t       g_spoke_position;

/* ── transition state ── */
static lv_obj_t *g_old_page;
static page_id_t g_old_page_id;
static bool      g_in_transition;

/* ── spoke chain definitions ── */
static page_id_t chain_health[] = { PAGE_ACTIVITY, PAGE_HEARTRATE };
static page_id_t chain_env[]    = { PAGE_ALTIMETER, PAGE_CLIMATE };

static page_id_t *page_manager_get_chain(spoke_dir_t dir, int32_t *count)
{
    switch (dir) {
    case SPOKE_RIGHT: *count = 2; return chain_health;
    case SPOKE_LEFT:  *count = 2; return chain_env;
    default:          *count = 0; return NULL;
    }
}

/* ── transition animation callbacks ── */
static void trans_translate_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void trans_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

static void trans_complete_cb(lv_anim_t *a)
{
    if (!g_old_page) return;
    const page_t *old_def = pages_config_get(g_old_page_id);
    if (old_def && old_def->destroy) old_def->destroy();
    lv_obj_delete(g_old_page);
    g_old_page = NULL;
    g_in_transition = false;
    /* 过渡结束后强制刷新新页，清除 overshoot 弹回时可能残留的旧页像素 */
    if (g_current_page) lv_obj_invalidate(g_current_page);
}

/* ── internal helper ── */
static void page_manager_switch_to(page_id_t id, trans_dir_t dir)
{
    const page_t *p = pages_config_get(id);
    if (!p || !p->create) return;
    if (g_in_transition) return;

    /* first page — no animation */
    if (!g_current_page) {
        g_current_page = p->create(lv_screen_active());
        g_active_page = id;
        return;
    }

    /* same page — skip */
    if (id == g_active_page) return;

    g_in_transition = true;
    g_old_page = g_current_page;
    g_old_page_id = g_active_page;

    /* create new page */
    g_current_page = p->create(lv_screen_active());
    g_active_page = id;

    int32_t new_start  = (dir == TRANS_FROM_RIGHT) ?  240 : -240;
    int32_t old_end    = (dir == TRANS_FROM_RIGHT) ? -240 :  240;

    /* position new page offscreen */
    lv_obj_set_style_translate_x(g_current_page, new_start, 0);

    lv_anim_t a;

    /* new page: springy overshoot entrance (280ms) */
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_current_page);
    lv_anim_set_values(&a, new_start, 0);
    lv_anim_set_exec_cb(&a, trans_translate_cb);
    lv_anim_set_duration(&a, 280);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);

    /* old page: ease_in — 先慢后快，像被拽住才松开 (300ms) */
    lv_anim_set_var(&a, g_old_page);
    lv_anim_set_values(&a, 0, old_end);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_completed_cb(&a, trans_complete_cb);
    lv_anim_start(&a);

    /* old page: fade to invisible (255→0, 280ms — 比翻译先结束，避免访问已删除对象) */
    lv_anim_set_var(&a, g_old_page);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, trans_opa_cb);
    lv_anim_set_duration(&a, 280);
    lv_anim_set_completed_cb(&a, NULL);
    lv_anim_start(&a);
}

/* ── public API ── */
void page_manager_init(void)
{
    g_state = STATE_AT_HUB;
    g_overlay_depth = 0;
    g_active_page = PAGE_WATCHFACE;
    g_spoke_position = 0;
    g_in_transition = false;

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
    page_manager_switch_to(PAGE_WATCHFACE, trans_backward(g_entry_dir));
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
    page_manager_switch_to(chain[0], trans_forward(dir));
    g_state = STATE_AT_SPOKE;
}

void page_manager_spoke_next(void)
{
    int32_t count;
    page_id_t *chain = page_manager_get_chain(g_entry_dir, &count);
    if (!chain || g_spoke_position + 1 >= count) return;

    g_spoke_position++;
    page_manager_switch_to(chain[g_spoke_position], trans_forward(g_entry_dir));
}

void page_manager_spoke_prev(void)
{
    if (g_spoke_position > 0) {
        g_spoke_position--;
        int32_t count;
        page_id_t *chain = page_manager_get_chain(g_entry_dir, &count);
        page_manager_switch_to(chain[g_spoke_position], trans_backward(g_entry_dir));
    } else {
        page_manager_go_home();
    }
}

void page_manager_push(page_id_t id)
{
    page_manager_switch_to(id, TRANS_FROM_RIGHT);
    g_state = STATE_AT_OVERLAY;
}

void page_manager_pop(void)
{
    page_manager_go_home();
}

void page_manager_update(void)
{
    if (g_in_transition) return;
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
