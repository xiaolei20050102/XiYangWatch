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

extern osThreadId_t lvglTaskHandle;  /* freertos.c 中定义, 用于通知新触摸数据 */

void I2CSensTask(void *pvParameters)
{
    CST816_Init();

    heartbeat_register(TASK_I2CSENS, 200);
    heartbeat_tick(TASK_I2CSENS);

    for (;;)
    {
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
        g_shm.touch.gesture = CST816_GetGesture();  /* 手势寄存器 (读后自动清零) */

        __DSB();
        osThreadFlagsSet(lvglTaskHandle, 0x01);//任务通知，较小开销通知数据采集完成

        heartbeat_tick(TASK_I2CSENS);
        osDelay(5);
    }
}
