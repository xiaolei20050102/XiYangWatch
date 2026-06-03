/**
 * @file    heartbeat.c
 * @brief   任务心跳监控实现 — CMSIS-RTOS v2
 */

#include "heartbeat.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"

/* ═══════════════════════════════════════════════════════════════
 *  心跳记录表 — 每个被监控的任务占一行
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t last_tick;    /* 最后一次报到的时间戳 (HAL_GetTick, 单位 ms) */
    uint32_t timeout_ms;   /* 允许的最大沉默时间                           */
    uint8_t  registered;   /* 1=已注册, 0=空位                            */
    uint8_t  dead_count;   /* 连续超时次数, 防抖                          */
} heartbeat_entry_t;

static heartbeat_entry_t s_heartbeats[MAX_MONITORED_TASKS];

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_register — 任务初始化时注册 (只调一次)
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_register(task_id_t id, uint32_t timeout_ms)
{
    if (id >= MAX_MONITORED_TASKS) return;

    s_heartbeats[id].last_tick   = HAL_GetTick();
    s_heartbeats[id].timeout_ms  = timeout_ms;
    s_heartbeats[id].registered  = 1;
    s_heartbeats[id].dead_count  = 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_tick — 任务报到 (主循环每周期调一次)
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_tick(task_id_t id)
{
    if (id >= MAX_MONITORED_TASKS) return;
    s_heartbeats[id].last_tick = HAL_GetTick();
}

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_monitor_all — 巡检全部任务 (DiagTask 每秒调用)
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_monitor_all(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        if (!s_heartbeats[i].registered) continue;

        if (now - s_heartbeats[i].last_tick > s_heartbeats[i].timeout_ms) {
            s_heartbeats[i].dead_count++;

            if (s_heartbeats[i].dead_count >= 3) {
                HAL_Delay(50);
                NVIC_SystemReset();
            }
        } else {
            s_heartbeats[i].dead_count = 0;
        }
    }
}
