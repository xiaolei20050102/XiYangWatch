#ifndef QMI8658_H
#define QMI8658_H

#include <stdint.h>

/* QMI8658 六轴 IMU 驱动 — BSP 层纯驱动, 不含 FreeRTOS
 *
 * I2C 地址: 0x6B (7-bit, AD0 接 GND)
 * 总线:     I2C1 (共享, 由 I2CSensTask 独占访问)
 *
 * 调用者: App/Framework/i2c_sens_task.c
 * 周期:   每 4 周期 (20ms = 50Hz)
 * 输出:   加速度 mg / 陀螺仪 0.1 dps → 写 g_shm.imu
 */

#define QMI8658_ADDR        0x6B
#define QMI8658_ADDR_8BIT   ((QMI8658_ADDR) << 1)
#define QMI8658_I2C         &hi2c1

#define QMI8658_I2C_WRITE(reg, pData, sz) \
    HAL_I2C_Mem_Write(QMI8658_I2C, QMI8658_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define QMI8658_I2C_READ(reg, pData, sz) \
    HAL_I2C_Mem_Read(QMI8658_I2C, QMI8658_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

uint8_t qmi8658_init(void);
uint8_t qmi8658_read(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz);
void    qmi8658_sleep(void);
void    qmi8658_wakeup(void);

#endif
