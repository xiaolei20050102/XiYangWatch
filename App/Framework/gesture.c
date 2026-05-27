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

    g_armed = false;
    g_last_gesture_tick = lv_tick_get();

    page_state_t state = page_manager_get_state();

    switch (state) {

    case STATE_AT_HUB:
        if (g == GESTURE_LEFT)       page_manager_go_spoke(SPOKE_LEFT);
        else if (g == GESTURE_RIGHT) page_manager_go_spoke(SPOKE_RIGHT);
        else if (g == GESTURE_UP)    page_manager_push_up(PAGE_MENU);
        else if (g == GESTURE_DOWN)  page_manager_go_spoke(SPOKE_DOWN);
        else if (g == GESTURE_LONGPRESS) page_manager_push(PAGE_WATCHFACE_SEL);
        break;

    case STATE_AT_SPOKE: {
        spoke_dir_t entry = page_manager_get_entry_dir();

        /* 同方向 = 继续深入辐条链 */
        int deeper = 0;
        if (entry == SPOKE_LEFT  && g == GESTURE_LEFT)  deeper = 1;
        if (entry == SPOKE_RIGHT && g == GESTURE_RIGHT) deeper = 1;
        if (entry == SPOKE_UP    && g == GESTURE_UP)    deeper = 1;
        if (entry == SPOKE_DOWN  && g == GESTURE_DOWN)  deeper = 1;
        if (deeper) { page_manager_spoke_next(); break; }

        /* 反方向 = 逐级回退 */
        int back = 0;
        if (entry == SPOKE_LEFT  && g == GESTURE_RIGHT) back = 1;
        if (entry == SPOKE_RIGHT && g == GESTURE_LEFT)  back = 1;
        if (entry == SPOKE_UP    && g == GESTURE_DOWN)  back = 1;
        if (entry == SPOKE_DOWN  && g == GESTURE_UP)    back = 1;
        if (back) page_manager_spoke_prev();
        break;
    }

    case STATE_AT_OVERLAY:
        if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
            page_manager_pop(g);
        break;
    }
}
