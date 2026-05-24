#ifndef DATA_PROVIDER_H
#define DATA_PROVIDER_H

#include <stdint.h>
#include <stdbool.h>

/* ── 时间 ── */
typedef struct {
    int hour, min, sec;
    int year, month, day;
    int weekday;
} watch_time_t;

void watch_data_get_time(watch_time_t *t);

/* ── 心率 ── */
int32_t watch_data_get_heart_rate(void);

/* ── 血氧 ── */
int32_t watch_data_get_spo2(void);
void    watch_data_spo2_start(void);
void    watch_data_spo2_abort(void);
bool    watch_data_spo2_is_done(void);
int32_t watch_data_spo2_history_count(void);
bool    watch_data_spo2_history_get(int32_t idx, int32_t *spo2, watch_time_t *t);

/* ── 活动 ── */
int32_t watch_data_get_steps(void);
int32_t watch_data_get_calories(void);
int32_t watch_data_get_distance_m(void);

/* ── 运动 ── */
typedef enum { WORKOUT_NONE, WORKOUT_RUN, WORKOUT_WALK, WORKOUT_CYCLE } workout_type_t;
typedef struct {
    workout_type_t type; int32_t duration_sec;
    int32_t avg_hr, steps, calories, distance_m; bool is_active;
} workout_session_t;

void watch_data_workout_start(workout_type_t type);
void watch_data_workout_pause(void);
void watch_data_workout_resume(void);
void watch_data_workout_stop(void);
void watch_data_workout_get(workout_session_t *s);
int32_t watch_data_workout_history_count(void);
bool    watch_data_workout_history_get(int32_t idx, workout_session_t *s);

/* ── 环境 ── */
int32_t watch_data_get_temperature(void);
int32_t watch_data_get_humidity(void);
int32_t watch_data_get_pressure(void);
int32_t watch_data_get_altitude(void);

/* ── 环境光 ── */
int32_t watch_data_get_ambient_lux(void);
bool    watch_data_is_auto_brightness(void);
void    watch_data_set_auto_brightness(bool on);
void    watch_data_set_brightness(int32_t pct);
int32_t watch_data_get_brightness(void);

/* ── 系统 ── */
int32_t watch_data_get_battery(void);
int32_t watch_data_get_screen_timeout(void);
void    watch_data_set_screen_timeout(int32_t sec);

/* ── 表盘样式 ── */
typedef enum { WATCHFACE_CLASSIC, WATCHFACE_ANALOG, WATCHFACE_MINIMAL } watchface_t;
watchface_t watch_data_get_watchface(void);
void        watch_data_set_watchface(watchface_t wf);
int32_t     watch_data_watchface_count(void);

/* ── 闹钟 ── */
typedef struct { int hour, min; bool enabled; bool repeat[7]; } watch_alarm_t;
int32_t watch_data_alarm_count(void);
bool    watch_data_alarm_get(int32_t idx, watch_alarm_t *a);
void    watch_data_alarm_set(int32_t idx, const watch_alarm_t *a);

/* ── 通知 ── */
typedef struct { char title[32], body[100]; uint32_t timestamp; bool unread; } watch_notification_t;
int32_t watch_data_notification_count(void);
bool    watch_data_notification_get(int32_t idx, watch_notification_t *n);
void    watch_data_notification_mark_read(int32_t idx);
void    watch_data_notification_clear_all(void);
bool    watch_data_notification_has_new(void);

/* ── OTA ── */
bool watch_data_ota_available(void);
int  watch_data_ota_progress(void);
void watch_data_ota_trigger(void);

/* ── 找手机 ── */
void watch_data_find_phone_trigger(void);

#endif
