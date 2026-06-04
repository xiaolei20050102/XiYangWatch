/**
 * @file    i2c_sens_task.c
 * @brief   I2CSensTask — I2C1 传感器调度 — CMSIS-RTOS v2
 */

#include "i2c_sens_task.h"
#include "cmsis_os2.h"
#include "touch_cst816s.h"     /* BSP/Touch/  — CST816_GetFingerNum */
#include "shared_memory.h"     /* App/Framework/ — g_shm            */
#include "heartbeat.h"         /* App/Framework/ — heartbeat         */
#include "log_port.h"          /* App/Framework/ — log_printf        */
#include "ds3231.h"            /* BSP/RTC/     — ds3231_read_time    */

extern osThreadId_t lvglTaskHandle;

void I2CSensTask(void *pvParameters)
{
    CST816_Init();
    ds3231_init();

    heartbeat_register(TASK_I2CSENS, 200);
    heartbeat_tick(TASK_I2CSENS);

    uint32_t counter = 0;

    for (;;)
    {
        /* ── 每周期: 触摸 + 手势 (5ms = 200Hz) ── */
        uint8_t finger = CST816_GetFingerNum();
        if (finger != 0x00 && finger != 0xFF) {
            Touch_Info_t info;
            CST816_GetTouch(&info);
            g_shm.touch.x = info.X_Pos;
            g_shm.touch.y = info.Y_Pos;
            g_shm.touch.pressed = 1;
        } else {
            g_shm.touch.pressed = 0;
        }
        g_shm.touch.gesture = CST816_GetGesture();

        /* ── 每 200 周期 (1s): RTC ── */
        if (counter % 200 == 0) {
            uint8_t h, m, s, d, mo, w;
            uint16_t y;
            ds3231_read_time(&h, &m, &s, &d, &mo, &w, &y);
            g_shm.time.hour    = h;
            g_shm.time.min     = m;
            g_shm.time.sec     = s;
            g_shm.time.day     = d;
            g_shm.time.month   = mo;
            g_shm.time.weekday = w;
            g_shm.time.year    = y + 2000;
            log_printf("[RTC] %04d-%02d-%02d %02d:%02d:%02d week=%d\r\n",
                       g_shm.time.year, g_shm.time.month, g_shm.time.day,
                       g_shm.time.hour, g_shm.time.min, g_shm.time.sec,
                       g_shm.time.weekday);
        }

        __DSB();
        osThreadFlagsSet(lvglTaskHandle, 0x01);

        heartbeat_tick(TASK_I2CSENS);
        counter++;
        osDelay(5);
    }
}
