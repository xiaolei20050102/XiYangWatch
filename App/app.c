#include "app.h"
#include "Framework/page_manager.h"
#include "Framework/gesture.h"

void app_init(void)
{
    gesture_init();
    page_manager_init();
}

void app_loop(void)
{
    page_manager_update();
}
