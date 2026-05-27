#ifndef PAGES_CONFIG_H
#define PAGES_CONFIG_H

#include "page.h"

typedef enum {
    PAGE_WATCHFACE = 0,
    PAGE_HEARTRATE,
    PAGE_SPO2,
    PAGE_CONTROL_CENTER,
    PAGE_NOTIFICATIONS,
    PAGE_MENU,
    PAGE_DISPLAY,
    PAGE_WATCHFACE_SEL,
    PAGE_ACTIVITY,
    PAGE_ABOUT,
    /* Phase 2 */
    PAGE_ALARM,
    PAGE_STOPWATCH,
    PAGE_TIMER,
    PAGE_CALCULATOR,
    PAGE_SPO2_HISTORY,
    PAGE_WORKOUT_HISTORY,
    /* Phase 3 */
    PAGE_BLUETOOTH,
    /* Phase 4 */
    PAGE_FIND_PHONE,
    PAGE_ALTIMETER,
    PAGE_CLIMATE,
    /* Menu app sub-pages (empty stubs, not implemented yet) */
    PAGE_APP_ACTIVITY,
    PAGE_APP_HEALTH,
    PAGE_APP_ENVIRONMENT,
    PAGE_APP_FOCUS,
    PAGE_APP_STOPWATCH,
    PAGE_APP_ALARM,
    PAGE_APP_CALCULATOR,
    PAGE_APP_FLASHLIGHT,
    PAGE_APP_SETTINGS,
    PAGE_COUNT
} page_id_t;

extern const page_t *g_pages[PAGE_COUNT];

const page_t *pages_config_get(page_id_t id);

#endif
