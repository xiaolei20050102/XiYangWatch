/**
 * @file    power_task.c
 * @brief   PowerTask — 电源管理 — CMSIS-RTOS v2
 */

#include "power_task.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "heartbeat.h"
#include "shared_memory.h"
#include "log_port.h"

void PowerTask(void *pvParameters)
{
    heartbeat_register(TASK_POWER, 3000);
    heartbeat_tick(TASK_POWER);

    for (;;)
    {
        /* TODO: ADC 读电池 + 充电检测 + 低功耗状态机 */
        g_shm.power.battery_pct = 85;

        log_printf("[POWER] battery=%d%% charging=%d\r\n",
                   g_shm.power.battery_pct,
                   g_shm.power.charging);

        heartbeat_tick(TASK_POWER);
        osDelay(1000);
    }
}
