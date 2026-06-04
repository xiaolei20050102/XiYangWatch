#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>

/* DS3231 RTC 驱动 — BSP 层纯驱动, 不含 FreeRTOS
 *
 * I2C 地址: 0x68 (7-bit)
 * 总线:     I2C1 (共享, 由 I2CSensTask 独占访问)
 *
 * 调用者: App/Framework/i2c_sens_task.c
 * 用法:   ds3231_init() → ds3231_read_time() 每 1s 读一次 → 写 g_shm.time
 */

/* ========================== 设备 & 外设定义 ========================== */

#define DS3231_ADDR         0x68    /* 7-bit I2C 地址 */
#define DS3231_ADDR_8BIT    ((DS3231_ADDR) << 1)   /* HAL 用的 8-bit 地址 */
#define DS3231_I2C          &hi2c1  /* I2C1 总线 (PB6 SCL, PB7 SDA) */

/* 时间寄存器 (BCD 编码) */
#define DS3231_REG_SEC      0x00    /* 秒   */
#define DS3231_REG_MIN      0x01    /* 分   */
#define DS3231_REG_HOUR     0x02    /* 时   */
#define DS3231_REG_DAY      0x04    /* 日   */
#define DS3231_REG_MONTH    0x05    /* 月   */
#define DS3231_REG_YEAR     0x06    /* 年   */

/*控制寄存器*/
#define DS3231_REG_CONTROL  0x0E          

/* ========================== 硬件抽象宏（移植时只改这里） ========================== */
#define RTC_I2C_WRITE(reg, pData, sz) \
    HAL_I2C_Mem_Write(DS3231_I2C, DS3231_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define RTC_I2C_READ(reg, pData, sz) \
    HAL_I2C_Mem_Read(DS3231_I2C, DS3231_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)



uint8_t ds3231_init(void);
uint8_t ds3231_read_time(uint8_t *hour, uint8_t *min, uint8_t *sec,
                          uint8_t *day, uint8_t *month, uint8_t *week,
                          uint16_t *year);
void    ds3231_set_time(uint8_t hour, uint8_t min, uint8_t sec,
                         uint8_t day, uint8_t month, uint8_t week,
                         uint16_t year);
void    ds3231_sleep(void);
void    ds3231_wakeup(void);

#endif
