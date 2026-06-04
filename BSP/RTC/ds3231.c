/**
 * @file    ds3231.c
 * @brief   DS3231 RTC 驱动 — BSP 层纯驱动，不含 FreeRTOS
 */

#include "ds3231.h"
#include "i2c.h"            /* hi2c1 */

/* ═══ BCD 编解码 ═══ */

static inline uint8_t bcd_to_dec(uint8_t val) {
    return ((val >> 4) & 0x0F) * 10 + (val & 0x0F);
}

static inline uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

/* ═══ 驱动函数 ═══ */

uint8_t ds3231_init(void)
{
    /* 初始化控制寄存器:
       bit7=0 晶振开, bit4~3=11, bit2=0 方波, 其他=0
       0x1C = 0b 0001 1100 */
    uint8_t ctrl = 0x1C;
    RTC_I2C_WRITE(0x0E, &ctrl, 1);
    return 0;
}

uint8_t ds3231_read_time(uint8_t *hour, uint8_t *min, uint8_t *sec,
                          uint8_t *day, uint8_t *month, uint8_t *week,
                          uint16_t *year)
{
    uint8_t buf[7];
    RTC_I2C_READ(DS3231_REG_SEC, buf, 7);  /* 从 0x00 连续读 7 字节 */

    *sec   = bcd_to_dec(buf[0]);
    *min   = bcd_to_dec(buf[1]);
    *hour  = bcd_to_dec(buf[2] & 0x3F);     /* 屏蔽 12/24h 标志位 */
    *week  = bcd_to_dec(buf[3]);             /* 1=周日 ~ 7=周六 */
    *day   = bcd_to_dec(buf[4]);
    *month = bcd_to_dec(buf[5] & 0x1F);     /* 屏蔽 century 位 */
    *year  = bcd_to_dec(buf[6]);

    return 0;
}

void ds3231_set_time(uint8_t hour, uint8_t min, uint8_t sec,
                      uint8_t day, uint8_t month, uint8_t week,
                      uint16_t year)
{
    uint8_t buf[7];
    buf[0] = dec_to_bcd(sec);
    buf[1] = dec_to_bcd(min);
    buf[2] = dec_to_bcd(hour);       /* 24 小时制 */
    buf[3] = dec_to_bcd(week);       /* 1=周日 ~ 7=周六 */
    buf[4] = dec_to_bcd(day);
    buf[5] = dec_to_bcd(month);
    buf[6] = dec_to_bcd((uint8_t)year);

    RTC_I2C_WRITE(DS3231_REG_SEC, buf, 7);  /* 一次写 7 字节 */
}

void ds3231_sleep(void) {
    /* DS3231 不需要特别处理 — I2C 总线 STOP 后自动低功耗 */
}

void ds3231_wakeup(void) {
    /* DS3231 不需要特别处理 — 下一次 I2C START 自动唤醒 */
}
