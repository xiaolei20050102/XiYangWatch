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
/* Menu app stubs */
extern const page_t page_app_activity;
extern const page_t page_app_health;
extern const page_t page_app_environment;
extern const page_t page_app_focus;
extern const page_t page_app_stopwatch;
extern const page_t page_app_alarm;
extern const page_t page_app_calculator;
extern const page_t page_app_flashlight;
extern const page_t page_app_settings;
extern const page_t page_app_heartrate;
extern const page_t page_app_heartrate_detail;
extern const page_t page_app_spo2;
extern const page_t page_app_spo2_detail;
extern const page_t page_app_activity_detail;
extern const page_t page_app_environment_detail;
extern const page_t page_app_environment_pressure_detail;

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
    [PAGE_APP_ACTIVITY]    = &page_app_activity,
    [PAGE_APP_HEALTH]      = &page_app_health,
    [PAGE_APP_ENVIRONMENT] = &page_app_environment,
    [PAGE_APP_FOCUS]       = &page_app_focus,
    [PAGE_APP_STOPWATCH]   = &page_app_stopwatch,
    [PAGE_APP_ALARM]       = &page_app_alarm,
    [PAGE_APP_CALCULATOR]  = &page_app_calculator,
    [PAGE_APP_FLASHLIGHT]  = &page_app_flashlight,
    [PAGE_APP_SETTINGS]    = &page_app_settings,
    [PAGE_APP_HEARTRATE]         = &page_app_heartrate,
    [PAGE_APP_HEARTRATE_DETAIL]  = &page_app_heartrate_detail,
    [PAGE_APP_SPO2]              = &page_app_spo2,
    [PAGE_APP_SPO2_DETAIL]       = &page_app_spo2_detail,
    [PAGE_APP_ACTIVITY_DETAIL]      = &page_app_activity_detail,
    [PAGE_APP_ENVIRONMENT_DETAIL]            = &page_app_environment_detail,
    [PAGE_APP_ENVIRONMENT_PRESSURE_DETAIL]   = &page_app_environment_pressure_detail,
};

const page_t *pages_config_get(page_id_t id)
{
    if (id >= PAGE_COUNT) return NULL;
    return g_pages[id];
}
