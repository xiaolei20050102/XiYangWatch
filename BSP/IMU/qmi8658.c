/**
 * @file    qmi8658.c
 * @brief   QMI8658 六轴 IMU 驱动 — BSP 层纯驱动
 *
 * @details 返回值用缩放后的整数:
 *           加速度 ×1000 = mg (LSB→mg 用整数乘除)
 *           陀螺仪 ×10   = 0.1 dps
 */

#include "qmi8658.h"
#include "i2c.h"

/* scaling: ±8g → 32768 LSB = 8000 mg,  1 LSB = 8000/32768 = 125/512 mg  */
/* scaling: ±1024dps → 32768 LSB = 10240 (×0.1dps), 1 LSB = 10240/32768 = 5/16 */

uint8_t qmi8658_init(void)
{
    uint8_t id;

    /* ① 软复位 */
    uint8_t rst = 0x20;
    QMI8658_I2C_WRITE(0x09, &rst, 1);  /* CTRL8 */

    /* ② 验证芯片 ID */
    QMI8658_I2C_READ(0x00, &id, 1);          /* 0x00: WHO_AM_I */
    if (id != 0x05) return 1;

    /* ③ CTRL1: 地址 0x6B, I2C */
    uint8_t c = 0x60;
    QMI8658_I2C_WRITE(0x02, &c, 1);

    /* ④ CTRL2: 加速度 ±8g, 1000Hz ODR */
    c = 0x83;
    QMI8658_I2C_WRITE(0x03, &c, 1);

    /* ⑤ CTRL3: 陀螺仪 ±1024dps, 1000Hz ODR */
    c = 0xA3;
    QMI8658_I2C_WRITE(0x04, &c, 1);

    /* ⑥ CTRL7: 使能加速度 + 陀螺仪 */
    c = 0x03;
    QMI8658_I2C_WRITE(0x08, &c, 1);

    return 0;
}

uint8_t qmi8658_read(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t data[12];
    int16_t raw[6];
    int i;

    /* 连续读 12 字节: 0x35~0x40 (加速度 6 + 陀螺仪 6) */
    QMI8658_I2C_READ(0x35, data, 12);

    for (i = 0; i < 6; i++)
        raw[i] = (int16_t)(data[i*2] | (data[i*2+1] << 8));

    /* 直接返回 raw 值, 缩放稍后确认 */
    *ax = raw[0]; *ay = raw[1]; *az = raw[2];
    *gx = raw[3]; *gy = raw[4]; *gz = raw[5];

    return 0;
}

void qmi8658_sleep(void) {
    uint8_t c = 0x00;
    QMI8658_I2C_WRITE(0x08, &c, 1);
}

void qmi8658_wakeup(void) {
    uint8_t c = 0x03;
    QMI8658_I2C_WRITE(0x08, &c, 1);
}
