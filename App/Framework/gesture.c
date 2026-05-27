#include "gesture.h"
#include "page_manager.h"
#include "lvgl.h"

static bool g_armed = true;
static uint32_t g_last_gesture_tick;
static gesture_intercept_cb_t g_intercept;

void gesture_init(void)
{
    /* 模拟器用键盘映射，STM32 用 CST816 手势 */
}

void gesture_set_intercept(gesture_intercept_cb_t cb)
{
    g_intercept = cb;
}

bool gesture_was_recent(void)
{
    return (lv_tick_get() - g_last_gesture_tick) < 300;
}

void gesture_feed(gesture_t g)
{
    /* 动画转换超时看门狗：必须在 lv_timer_handler 之前检查，
       否则若动画回调永不触发，lv_timer_handler 之后的检查也永远不会执行 */
    page_manager_check_timeout();

    if (g == GESTURE_NONE) {
        if (lv_tick_get() - g_last_gesture_tick > 200)
            g_armed = true;
        return;
    }
    if (!g_armed) return;

    /* 页面可注册拦截器，在测量等场景下吞掉手势 */
    if (g_intercept && g_intercept(g)) {
        g_armed = false;
        g_last_gesture_tick = lv_tick_get();
        return;
    }

    page_state_t state = page_manager_get_state();
    bool handled = false;

    switch (state) {

    case STATE_AT_HUB:
        if (g == GESTURE_LEFT)       { page_manager_go_spoke(SPOKE_LEFT);  handled = true; }
        else if (g == GESTURE_RIGHT) { page_manager_go_spoke(SPOKE_RIGHT); handled = true; }
        else if (g == GESTURE_UP)    { page_manager_push_up(PAGE_MENU);    handled = true; }
        else if (g == GESTURE_DOWN)  { page_manager_go_spoke(SPOKE_DOWN);  handled = true; }
        else if (g == GESTURE_LONGPRESS) { page_manager_push(PAGE_WATCHFACE_SEL); handled = true; }
        break;

    case STATE_AT_SPOKE: {
        spoke_dir_t entry = page_manager_get_entry_dir();

        /* 同方向 = 继续深入辐条链 */
        if (entry == SPOKE_LEFT  && g == GESTURE_LEFT)  { page_manager_spoke_next(); handled = true; break; }
        if (entry == SPOKE_RIGHT && g == GESTURE_RIGHT) { page_manager_spoke_next(); handled = true; break; }
        if (entry == SPOKE_UP    && g == GESTURE_UP)    { page_manager_spoke_next(); handled = true; break; }
        if (entry == SPOKE_DOWN  && g == GESTURE_DOWN)  { page_manager_spoke_next(); handled = true; break; }

        /* 反方向 = 逐级回退 */
        if (entry == SPOKE_LEFT  && g == GESTURE_RIGHT) { page_manager_spoke_prev(); handled = true; break; }
        if (entry == SPOKE_RIGHT && g == GESTURE_LEFT)  { page_manager_spoke_prev(); handled = true; break; }
        if (entry == SPOKE_UP    && g == GESTURE_DOWN)  { page_manager_spoke_prev(); handled = true; break; }
        if (entry == SPOKE_DOWN  && g == GESTURE_UP)    { page_manager_spoke_prev(); handled = true; break; }
        break;
    }

    case STATE_AT_OVERLAY:
        if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
            { page_manager_pop(g); handled = true; }
        break;
    }

    /* 只有真正触发了导航的手势才 disarm，防止滚动等无效手势消耗 g_armed */
    if (handled) {
        g_armed = false;
        g_last_gesture_tick = lv_tick_get();
    }
}
