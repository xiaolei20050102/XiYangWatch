#include "data_provider.h"
#include <string.h>

/* Phase 1: 使用假数据，后续硬件就绪后改为 0 */
#define USE_FAKE_DATA  1

/* ================================================================
 *  时间
 * ================================================================ */
#if USE_FAKE_DATA
    void watch_data_get_time(watch_time_t *t)
    {
        t->hour = 22; t->min = 45; t->sec = 30;
        t->year = 2026; t->month = 5; t->day = 23;
        t->weekday = 6; /* Saturday */
    }
#endif

/* ================================================================
 *  心率
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_heart_rate(void) { return 75; }
#endif

/* ================================================================
 *  血氧
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_spo2(void)       { return -1; }
    void    watch_data_spo2_start(void)      {}
    void    watch_data_spo2_abort(void)      {}
    bool    watch_data_spo2_is_done(void)    { return false; }
    int32_t watch_data_spo2_history_count(void) { return 0; }
    bool    watch_data_spo2_history_get(int32_t idx, int32_t *spo2, watch_time_t *t) { return false; }
#endif

/* ================================================================
 *  活动
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_steps(void)        {
        static int32_t steps;
        steps += 123;
        if (steps > 10000) steps = 10000;
        return steps;
    }
    int32_t watch_data_get_calories(void)     { return 320; }
    int32_t watch_data_get_distance_m(void)   { return 5800; }
#endif

/* ================================================================
 *  运动
 * ================================================================ */
#if USE_FAKE_DATA
    void watch_data_workout_start(workout_type_t type) {}
    void watch_data_workout_pause(void)                {}
    void watch_data_workout_resume(void)               {}
    void watch_data_workout_stop(void)                 {}
    void watch_data_workout_get(workout_session_t *s)  { memset(s, 0, sizeof(*s)); }
    int32_t watch_data_workout_history_count(void)     { return 0; }
    bool    watch_data_workout_history_get(int32_t idx, workout_session_t *s) { return false; }
#endif

/* ================================================================
 *  环境
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_temperature(void)  { return 225; }
    int32_t watch_data_get_humidity(void)     { return 55; }
    int32_t watch_data_get_pressure(void)     { return 1013; }
    int32_t watch_data_get_altitude(void)     { return 156; }
#endif

/* ================================================================
 *  环境光
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_ambient_lux(void)        { return 300; }
    bool    watch_data_is_auto_brightness(void)      { return true; }
    void    watch_data_set_auto_brightness(bool on)  {}
    void    watch_data_set_brightness(int32_t pct)   {}
    int32_t watch_data_get_brightness(void)          { return 70; }
#endif

/* ================================================================
 *  系统
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_get_battery(void)               { return 85; }
    int32_t watch_data_get_screen_timeout(void)        { return 30; }
    void    watch_data_set_screen_timeout(int32_t sec) {}
#endif

/* ================================================================
 *  表盘样式
 * ================================================================ */
#if USE_FAKE_DATA
    watchface_t watch_data_get_watchface(void)       { return WATCHFACE_CLASSIC; }
    void        watch_data_set_watchface(watchface_t wf) {}
    int32_t     watch_data_watchface_count(void)      { return 3; }
#endif

/* ================================================================
 *  闹钟
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_alarm_count(void)                           { return 0; }
    bool    watch_data_alarm_get(int32_t idx, watch_alarm_t *a)    { return false; }
    void    watch_data_alarm_set(int32_t idx, const watch_alarm_t *a) {}
#endif

/* ================================================================
 *  通知
 * ================================================================ */
#if USE_FAKE_DATA
    int32_t watch_data_notification_count(void)                      { return 0; }
    bool    watch_data_notification_get(int32_t idx, watch_notification_t *n) { return false; }
    void    watch_data_notification_mark_read(int32_t idx)           {}
    void    watch_data_notification_clear_all(void)                  {}
    bool    watch_data_notification_has_new(void)                    { return false; }
#endif

/* ================================================================
 *  OTA
 * ================================================================ */
#if USE_FAKE_DATA
    bool watch_data_ota_available(void)  { return false; }
    int  watch_data_ota_progress(void)   { return 0; }
    void watch_data_ota_trigger(void)    {}
#endif

/* ================================================================
 *  找手机
 * ================================================================ */
#if USE_FAKE_DATA
    void watch_data_find_phone_trigger(void) {}
#endif
