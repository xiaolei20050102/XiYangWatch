/**
 * @file    bme280.c
 * @brief   BME280 温湿度气压传感器驱动 — BSP 层纯驱动
 */

#include "bme280.h"
#include "i2c.h"

/* ═══ 补偿系数 (init 时读入, 之后不变) ═══ */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;
static int32_t  t_fine;


uint8_t bme280_init(void)
{
    uint8_t id, buf[26], h_buf[7];

    /* ① 验证芯片 ID */
    BME280_I2C_READ(0xD0, &id, 1);
    if (id != 0x60) return 1;

    /* ② 软复位 + 等待 NVM 加载 */
    uint8_t rst = 0xB6;
    BME280_I2C_WRITE(0xE0, &rst, 1);
    HAL_Delay(10);

    /* ③ 读补偿系数 (0x88~0xA1, 26 bytes) */
    BME280_I2C_READ(0x88, buf, 26);
    dig_T1 =  buf[0]  | (buf[1]  << 8);
    dig_T2 =  buf[2]  | (buf[3]  << 8);
    dig_T3 =  buf[4]  | (buf[5]  << 8);
    dig_P1 =  buf[6]  | (buf[7]  << 8);
    dig_P2 =  buf[8]  | (buf[9]  << 8);
    dig_P3 =  buf[10] | (buf[11] << 8);
    dig_P4 =  buf[12] | (buf[13] << 8);
    dig_P5 =  buf[14] | (buf[15] << 8);
    dig_P6 =  buf[16] | (buf[17] << 8);
    dig_P7 =  buf[18] | (buf[19] << 8);
    dig_P8 =  buf[20] | (buf[21] << 8);
    dig_P9 =  buf[22] | (buf[23] << 8);
    dig_H1 =  buf[25];

    /* ④ 读湿度补偿系数 (0xE1~0xE7, 7 bytes) */
    BME280_I2C_READ(0xE1, h_buf, 7);
    dig_H2 = h_buf[0] | (h_buf[1] << 8);
    dig_H3 = h_buf[2];
    dig_H4 = (int16_t)((h_buf[3] << 4) | (h_buf[4] & 0x0F));
    dig_H5 = (int16_t)((h_buf[5] << 4) | (h_buf[4] >> 4));
    dig_H6 = (int8_t)h_buf[6];

    /* ⑤ 配置: humidity x1, temp x1, press x1, forced mode */
    uint8_t cfg;
    cfg = 0x01;  BME280_I2C_WRITE(0xF2, &cfg, 1);   /* ctrl_hum  */
    cfg = 0x25;  BME280_I2C_WRITE(0xF4, &cfg, 1);   /* t x1, p x1, forced */
    cfg = 0x14;  BME280_I2C_WRITE(0xF5, &cfg, 1);   /* config    */

    return 0;
}

uint8_t bme280_read(int32_t *temperature, uint32_t *humidity, uint32_t *pressure)
{
    uint8_t raw[8];
    int32_t adc_T, adc_P, adc_H;

    /* ① 触发测量 */
    uint8_t cfg = 0x25;
    BME280_I2C_WRITE(0xF4, &cfg, 1);

    /* ② 等待完成 (最多 10 次, 正常 2~3 次) */
    uint8_t status;
    int timeout = 10;
    do {
        BME280_I2C_READ(0xF3, &status, 1);
    } while ((status & 0x08) && --timeout > 0);
    if (timeout == 0) return 1;  /* 超时放弃 */

    /* ③ 读原始数据 */
    BME280_I2C_READ(0xF7, raw, 8);
    adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
    adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);
    adc_H = ((int32_t)raw[6] << 8)  |  (int32_t)raw[7];

    /* ═══ 温度补偿 ═══ */
    {
        int32_t var1, var2;
        var1 = (((adc_T >> 3) - ((int32_t)dig_T1 << 1)) * (int32_t)dig_T2) >> 11;
        var2 = ((((adc_T >> 4) - (int32_t)dig_T1) * ((adc_T >> 4) - (int32_t)dig_T1)) >> 12)
               * (int32_t)dig_T3 >> 14;
        t_fine = var1 + var2;
        *temperature = (t_fine * 5 + 128) >> 8;
    }

    /* ═══ 气压补偿 (Bosch BME280 64-bit — Adafruit/官方完全一致) ═══ */
    {
        int64_t var1, var2, p;
        var1 = (int64_t)t_fine - 128000;
        var2 = var1 * var1 * (int64_t)dig_P6;
        var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
        var2 = var2 + ((int64_t)dig_P4 << 35);
        var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8)
             + ((var1 * (int64_t)dig_P2) << 12);
        var1 = ((((int64_t)1 << 47) + var1) * (int64_t)dig_P1) >> 33;
        if (var1 == 0) { *pressure = 0; return 1; }
        p = 1048576 - adc_P;
        p = (((p << 31) - var2) * 3125) / var1;
        var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
        var2 = ((int64_t)dig_P8 * p) >> 19;
        p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);
        *pressure = (uint32_t)(p >> 8);  /* x1 过采样需额外除 256 */
    }

    /* ═══ 湿度补偿 (BME280 数据手册附录 A) ═══ */
    {
        int32_t v_x1_u32r;
        v_x1_u32r = (t_fine - ((int32_t)76800));
        v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15)
                     * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10)
                            * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10)
                          + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
        v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
                                     * ((int32_t)dig_H1)) >> 4));
        v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
        v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
        *humidity = (uint32_t)(v_x1_u32r >> 12) * 100 / 1024;
    }

    return 0;
}

void bme280_sleep(void) {
    uint8_t cfg = 0x00;
    BME280_I2C_WRITE(0xF4, &cfg, 1);
}

void bme280_wakeup(void) {
    uint8_t cfg = 0x25;
    BME280_I2C_WRITE(0xF4, &cfg, 1);
}
