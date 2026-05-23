#include "gesture.h"
#include "page_manager.h"

void gesture_init(void)
{
    /* 模拟器用键盘映射，STM32 用 CST816 手势 */
}

void gesture_feed(gesture_t g)
{
    if (g == GESTURE_NONE) return;

    page_state_t state = page_manager_get_state();

    switch (state) {

    case STATE_AT_HUB:
        if (g == GESTURE_LEFT)       page_manager_go_spoke(SPOKE_LEFT);
        else if (g == GESTURE_RIGHT) page_manager_go_spoke(SPOKE_RIGHT);
        else if (g == GESTURE_UP)    page_manager_go_spoke(SPOKE_UP);
        else if (g == GESTURE_DOWN)  page_manager_go_spoke(SPOKE_DOWN);
        else if (g == GESTURE_LONGPRESS) page_manager_push(PAGE_WATCHFACE_SEL);
        break;

    case STATE_AT_SPOKE:
        if (g != GESTURE_NONE) page_manager_go_home();
        break;

    case STATE_AT_OVERLAY:
        if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
            page_manager_pop();
        break;
    }
}
