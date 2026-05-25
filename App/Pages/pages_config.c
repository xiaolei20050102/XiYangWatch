#include "pages_config.h"

/* 各页面 page_t 外部声明 */
extern const page_t page_watchface;
extern const page_t page_heartrate;
extern const page_t page_spo2;
extern const page_t page_control_center;
extern const page_t page_notifications;
extern const page_t page_menu;
extern const page_t page_display;
extern const page_t page_watchface_sel;
extern const page_t page_activity;
extern const page_t page_about;
/* Phase 2 */
extern const page_t page_alarm;
extern const page_t page_stopwatch;
extern const page_t page_timer;
extern const page_t page_calculator;
extern const page_t page_spo2_history;
extern const page_t page_workout_history;
/* Phase 3 */
extern const page_t page_bluetooth;
/* Phase 4 */
extern const page_t page_find_phone;
extern const page_t page_altimeter;
extern const page_t page_climate;

const page_t *g_pages[PAGE_COUNT] = {
    [PAGE_WATCHFACE]       = &page_watchface,
    [PAGE_HEARTRATE]       = &page_heartrate,
    [PAGE_SPO2]            = &page_spo2,
    [PAGE_CONTROL_CENTER]  = &page_control_center,
    [PAGE_NOTIFICATIONS]   = &page_notifications,
    [PAGE_MENU]            = &page_menu,
    [PAGE_DISPLAY]         = &page_display,
    [PAGE_WATCHFACE_SEL]   = &page_watchface_sel,
    [PAGE_ACTIVITY]        = &page_activity,
    [PAGE_ABOUT]           = &page_about,
    [PAGE_ALARM]           = &page_alarm,
    [PAGE_STOPWATCH]       = &page_stopwatch,
    [PAGE_TIMER]           = &page_timer,
    [PAGE_CALCULATOR]      = &page_calculator,
    [PAGE_SPO2_HISTORY]    = &page_spo2_history,
    [PAGE_WORKOUT_HISTORY] = &page_workout_history,
    [PAGE_BLUETOOTH]       = &page_bluetooth,
    [PAGE_FIND_PHONE]      = &page_find_phone,
    [PAGE_ALTIMETER]       = &page_altimeter,
    [PAGE_CLIMATE]         = &page_climate,
};

const page_t *pages_config_get(page_id_t id)
{
    if (id >= PAGE_COUNT) return NULL;
    return g_pages[id];
}
