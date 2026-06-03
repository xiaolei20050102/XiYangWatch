#include "power_task.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"           /* vTaskDelay, pdMS_TO_TICKS */
#include "stm32f4xx_hal.h"  /* HAL_GetTick, HAL_IWDG, HAL_UART, __HAL_RCC */
#include "heartbeat.h"      /* heartbeat_register / heartbeat_tick / heartbeat_monitor_all */
#include "shared_memory.h"
#include "log_port.h" 



void PowerTask(void *pvParameters) {
    heartbeat_register(TASK_POWER, 3000);
    heartbeat_tick(TASK_POWER);
    for (;;) {

        g_shm.power.battery_pct = 85;

        log_printf("[POWER] battery=%d%% charging=%d\r\n",
                                g_shm.power.battery_pct,
                                g_shm.power.charging);


        heartbeat_tick(TASK_POWER);
        /* ④ 阻塞 1 秒 — 必须有阻塞点, 不能空转 */
        vTaskDelay(pdMS_TO_TICKS(1000));        
    }
}