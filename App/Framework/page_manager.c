#include "page_manager.h"
#include "status_bar.h"
#include "wallpaper.h"
#include "lvgl.h"

#define OVERLAY_STACK_MAX 8

typedef enum {
    TRANS_FROM_RIGHT,
    TRANS_FROM_LEFT,
    TRANS_FROM_BOTTOM,
    TRANS_FADE,
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
static lv_obj_t     *g_wallpaper;
static page_id_t     g_active_page;
static spoke_dir_t   g_entry_dir;
static int32_t       g_spoke_position;

/* ── transition state ── */
static lv_obj_t *g_old_page;
static page_id_t g_old_page_id;
static bool      g_in_transition;
static uint32_t  g_transition_start_tick;
#define TRANSITION_TIMEOUT_MS 500
/* g_overlay_stack + g_overlay_depth 组成层叠栈，push时压入父页，pop时弹出 */

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

static void trans_translate_y_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

static void trans_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

static void trans_entrance_done_cb(lv_anim_t *a)
{
    if (!g_current_page) return;
    lv_obj_set_style_translate_x(g_current_page, 0, 0);
    lv_obj_set_style_translate_y(g_current_page, 0, 0);
    lv_obj_invalidate(g_current_page);
}

static void trans_complete_cb(lv_anim_t *a)
{
    if (!g_old_page) return;
    const page_t *old_def = pages_config_get(g_old_page_id);
    if (old_def && old_def->destroy) old_def->destroy();
    lv_obj_delete(g_old_page);
    g_old_page = NULL;
    g_in_transition = false;

    /* hide wallpaper after transition if new page is opaque overlay */
    const page_t *cur = pages_config_get(g_active_page);
    if (cur && cur->type == PAGE_TYPE_OVERLAY)
        lv_obj_add_flag(g_wallpaper, LV_OBJ_FLAG_HIDDEN);

    if (g_current_page) {
        lv_obj_set_style_translate_x(g_current_page, 0, 0);
        lv_obj_set_style_translate_y(g_current_page, 0, 0);
        lv_obj_invalidate(g_current_page);
    }
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
        lv_obj_remove_flag(g_current_page, LV_OBJ_FLAG_SCROLLABLE);
        g_active_page = id;
        status_bar_set_visible(id != PAGE_WATCHFACE);
        status_bar_bring_to_front();
        return;
    }

    /* same page — skip */
    if (id == g_active_page) return;

    g_in_transition = true;
    g_transition_start_tick = lv_tick_get();
    g_old_page = g_current_page;
    g_old_page_id = g_active_page;

    /* create new page */
    g_current_page = p->create(lv_screen_active());
    lv_obj_remove_flag(g_current_page, LV_OBJ_FLAG_SCROLLABLE);
    g_active_page = id;
    status_bar_set_visible(id != PAGE_WATCHFACE);
    status_bar_bring_to_front();

    /* ensure wallpaper visible during transition (hide after animation completes if needed) */
    lv_obj_remove_flag(g_wallpaper, LV_OBJ_FLAG_HIDDEN);

    bool is_bottom = (dir == TRANS_FROM_BOTTOM);
    bool is_fade   = (dir == TRANS_FADE);
    int32_t new_start  = is_bottom ? 280 : ((dir == TRANS_FROM_RIGHT) ?  240 : -240);
    int32_t old_end    = is_bottom ? 0   : ((dir == TRANS_FROM_RIGHT) ? -240 :  240);

    /* position new page offscreen */
    if (is_bottom)
        lv_obj_set_style_translate_y(g_current_page, new_start, 0);
    else if (!is_fade)
        lv_obj_set_style_translate_x(g_current_page, new_start, 0);

    lv_anim_t a_entrance, a_exit, a_fade;

    if (is_fade) {
        /* fade-only: new page fades in, no translate */
        lv_obj_set_style_opa(g_current_page, LV_OPA_TRANSP, 0);

        lv_anim_init(&a_entrance);
        lv_anim_set_var(&a_entrance, g_current_page);
        lv_anim_set_values(&a_entrance, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a_entrance, trans_opa_cb);
        lv_anim_set_duration(&a_entrance, 260);
        lv_anim_set_path_cb(&a_entrance, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a_entrance, trans_entrance_done_cb);
        lv_anim_start(&a_entrance);

        /* old page: fade out in place */
        lv_anim_init(&a_fade);
        lv_anim_set_var(&a_fade, g_old_page);
        lv_anim_set_values(&a_fade, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&a_fade, trans_opa_cb);
        lv_anim_set_path_cb(&a_fade, lv_anim_path_ease_out);
        lv_anim_set_duration(&a_fade, 260);
        lv_anim_set_completed_cb(&a_fade, trans_complete_cb);
        lv_anim_start(&a_fade);
    } else {
        /* new page: springy overshoot entrance (280ms) */
        lv_anim_init(&a_entrance);
        lv_anim_set_var(&a_entrance, g_current_page);
        lv_anim_set_values(&a_entrance, new_start, 0);
        lv_anim_set_exec_cb(&a_entrance, is_bottom ? trans_translate_y_cb : trans_translate_cb);
        lv_anim_set_duration(&a_entrance, 280);
        lv_anim_set_path_cb(&a_entrance, lv_anim_path_overshoot);
        lv_anim_set_completed_cb(&a_entrance, trans_entrance_done_cb);
        lv_anim_start(&a_entrance);

        /* old page: translate + fade out */
        if (is_bottom) {
            lv_anim_init(&a_fade);
            lv_anim_set_var(&a_fade, g_old_page);
            lv_anim_set_values(&a_fade, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_exec_cb(&a_fade, trans_opa_cb);
            lv_anim_set_path_cb(&a_fade, lv_anim_path_ease_out);
            lv_anim_set_duration(&a_fade, 280);
            lv_anim_set_completed_cb(&a_fade, trans_complete_cb);
            lv_anim_start(&a_fade);
        } else {
            lv_anim_init(&a_exit);
            lv_anim_set_var(&a_exit, g_old_page);
            lv_anim_set_values(&a_exit, 0, old_end);
            lv_anim_set_exec_cb(&a_exit, trans_translate_cb);
            lv_anim_set_path_cb(&a_exit, lv_anim_path_ease_in);
            lv_anim_set_duration(&a_exit, 300);
            lv_anim_set_completed_cb(&a_exit, trans_complete_cb);
            lv_anim_start(&a_exit);

            lv_anim_init(&a_fade);
            lv_anim_set_var(&a_fade, g_old_page);
            lv_anim_set_values(&a_fade, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_exec_cb(&a_fade, trans_opa_cb);
            lv_anim_set_path_cb(&a_fade, lv_anim_path_ease_out);
            lv_anim_set_duration(&a_fade, 280);
            lv_anim_start(&a_fade);
        }
    }
}

/* ── public API ── */
void page_manager_init(void)
{
    g_state = STATE_AT_HUB;
    g_overlay_depth = 0;
    g_active_page = PAGE_WATCHFACE;
    g_spoke_position = 0;
    g_in_transition = false;

    /* 屏幕背景：壁纸图片 + 黑色底色兜底 */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

    g_wallpaper = lv_image_create(lv_screen_active());
    lv_image_set_src(g_wallpaper, &wallpaper_img);
    lv_obj_set_pos(g_wallpaper, 0, 0);

    const page_t *p = pages_config_get(PAGE_WATCHFACE);
    if (p && p->create) {
        g_current_page = p->create(lv_screen_active());
        lv_obj_remove_flag(g_current_page, LV_OBJ_FLAG_SCROLLABLE);
    }

    status_bar_create(lv_screen_active());
}

void page_manager_go_home(void)
{
    if (g_in_transition) return;
    page_manager_switch_to(PAGE_WATCHFACE, trans_backward(g_entry_dir));
    g_state = STATE_AT_HUB;
    g_spoke_position = 0;
}

void page_manager_go_spoke(spoke_dir_t dir)
{
    if (g_in_transition) return;
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
    if (g_in_transition) return;
    int32_t count;
    page_id_t *chain = page_manager_get_chain(g_entry_dir, &count);
    if (!chain || g_spoke_position + 1 >= count) return;

    g_spoke_position++;
    page_manager_switch_to(chain[g_spoke_position], trans_forward(g_entry_dir));
}

void page_manager_spoke_prev(void)
{
    if (g_in_transition) return;
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
    if (g_in_transition) return;
    if (g_overlay_depth >= OVERLAY_STACK_MAX) return;
    g_overlay_stack[g_overlay_depth++] = g_active_page;
    page_manager_switch_to(id, TRANS_FROM_RIGHT);
    g_state = STATE_AT_OVERLAY;
}

void page_manager_push_up(page_id_t id)
{
    if (g_in_transition) return;
    if (g_overlay_depth >= OVERLAY_STACK_MAX) return;
    g_overlay_stack[g_overlay_depth++] = g_active_page;
    page_manager_switch_to(id, TRANS_FROM_BOTTOM);
    g_state = STATE_AT_OVERLAY;
}

void page_manager_push_fade(page_id_t id)
{
    if (g_in_transition) return;
    if (g_overlay_depth >= OVERLAY_STACK_MAX) return;
    g_overlay_stack[g_overlay_depth++] = g_active_page;
    page_manager_switch_to(id, TRANS_FADE);
    g_state = STATE_AT_OVERLAY;
}

void page_manager_pop(gesture_t g)
{
    if (g_in_transition) return;
    if (g_overlay_depth == 0) return;
    g_overlay_depth--;
    /* 退回 overlay（如菜单）→ 原地淡入淡出；退回 hub → 顺着手势滑动 */
    trans_dir_t dir;
    if (g_overlay_depth > 0)
        dir = TRANS_FADE;
    else
        dir = (g == GESTURE_LEFT) ? TRANS_FROM_RIGHT : TRANS_FROM_LEFT;
    page_manager_switch_to(g_overlay_stack[g_overlay_depth], dir);
    g_state = (g_overlay_depth > 0) ? STATE_AT_OVERLAY : STATE_AT_HUB;
}

void page_manager_check_timeout(void)
{
    if (g_in_transition && lv_tick_elaps(g_transition_start_tick) > TRANSITION_TIMEOUT_MS) {
        if (g_old_page) {
            const page_t *old_def = pages_config_get(g_old_page_id);
            if (old_def && old_def->destroy) old_def->destroy();
            lv_obj_delete(g_old_page);
            g_old_page = NULL;
        }
        if (g_current_page) {
            lv_anim_delete(g_current_page, NULL);
            lv_obj_set_style_translate_x(g_current_page, 0, 0);
            lv_obj_set_style_translate_y(g_current_page, 0, 0);
            lv_obj_invalidate(g_current_page);
        }
        g_in_transition = false;
    }
}

void page_manager_update(void)
{
    static uint32_t last_time_tick = 0;
    static uint32_t last_batt_tick = 0;
    uint32_t now = lv_tick_get();

    if (lv_tick_elaps(last_time_tick) >= 1000) {
        status_bar_refresh_time();
        last_time_tick = now;
    }
    if (lv_tick_elaps(last_batt_tick) >= 1000) {
        status_bar_refresh_battery();
        last_batt_tick = now;
    }

    page_manager_check_timeout();

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
