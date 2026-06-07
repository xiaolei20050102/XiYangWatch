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
#include "bme280.h"
#include "qmi8658.h"
#include "max30102.h"            /* BSP/ENV/     — bme280_read          */

extern osThreadId_t lvglTaskHandle;

void I2CSensTask(void *pvParameters)
{
    CST816_Init();
    ds3231_init();
    /* 先设时间 (BME280 init 可能影响 I2C 总线) */
    ds3231_set_time(16, 0, 0, 7, 6, 7, 26);  /* 时 分 秒 日 月 星期 年 */
    bme280_init();
    qmi8658_init();
    max30102_init();

    heartbeat_register(TASK_I2CSENS, 200);
    heartbeat_tick(TASK_I2CSENS);

    uint32_t counter = 0;

    for (;;)
    {
        /* ── I2C 总线恢复: 上一个操作超时后 HAL 可能锁死总线 ── */
        if (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) {
            HAL_I2C_DeInit(&hi2c1);
            MX_I2C1_Init();
            log_printf("[I2C] bus recovered\r\n");
        }

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

        if (counter % 4 == 0) {
            int16_t ax, ay, az, gx, gy, gz;
            qmi8658_read(&ax, &ay, &az, &gx, &gy, &gz);
            g_shm.imu.accel_x = ax;
            g_shm.imu.accel_y = ay;
            g_shm.imu.accel_z = az;
            g_shm.imu.gyro_x  = gx;
            g_shm.imu.gyro_y  = gy;
            g_shm.imu.gyro_z  = gz;
           // log_printf("[IMU] ax=%d ay=%d az=%d\r\n", ax, ay, az);
        }

        if (counter % 20 == 0) {
            uint32_t red, ir;
            if (max30102_read(&red, &ir) == 0) {
                g_shm.hr.hr_bpm = 0;  /* TODO: 算法 */
                g_shm.hr.hr_valid = (red > 5000);
                log_printf("[HR] red=%lu ir=%lu\r\n", red, ir);
            }
        }

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

            int32_t temp;
            uint32_t hum, press;
            if (bme280_read(&temp, &hum, &press) == 0) {
                g_shm.env.temperature = (int16_t)(temp / 10);
                g_shm.env.humidity    = (uint16_t)(hum / 10);
                g_shm.env.pressure    = press;
                /* 气压→海拔 (纯整数, 不耗栈): P₀=101325Pa, 每1Pa≈8.4cm */
                g_shm.env.altitude = (int16_t)(((int32_t)101325 - (int32_t)press) * 843 / 10000);
                log_printf("[ENV] T=%d.%dC H=%d.%d%% P=%luPa Alt=%dm\r\n",
                           temp / 100, (temp % 100) / 10,
                           hum / 100, (hum % 100) / 10,
                           press, g_shm.env.altitude);
            } else {
                log_printf("[ENV] read failed\r\n");
                g_shm.sens_health.env = SENS_RETRYING;
            }
        }

        __DSB();
        osThreadFlagsSet(lvglTaskHandle, 0x01);

        heartbeat_tick(TASK_I2CSENS);
        counter++;
        osDelay(5);
    }
}
