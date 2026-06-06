#ifndef BME280_H
#define BME280_H

#include <stdint.h>

/* BME280 环境传感器驱动 — BSP 层纯驱动, 不含 FreeRTOS
 *
 * I2C 地址: 0x76 (7-bit, SDO 接 GND)
 * 总线:     I2C1 (共享, 由 I2CSensTask 独占访问)
 *
 * 调用者: App/Framework/i2c_sens_task.c
 * 用法:   bme280_init() → bme280_read(&temp, &hum, &press) → 写 g_shm.env
 *
 * 返回值:
 *   temperature: ℃ × 100 (2250 = 22.50℃)
 *   humidity:    % × 100  (5520 = 55.20%)
 *   pressure:    Pa
 */

/* ========================== 设备 & 外设定义 ========================== */

#define BME280_ADDR         0x76
#define BME280_ADDR_8BIT    ((BME280_ADDR) << 1)
#define BME280_I2C          &hi2c1

/* ========================== 硬件抽象宏 ========================== */

#define BME280_I2C_WRITE(reg, pData, sz) \
    HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define BME280_I2C_READ(reg, pData, sz) \
    HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

/* ========================== 函数声明 ========================== */

uint8_t bme280_init(void);
uint8_t bme280_read(int32_t *temperature, uint32_t *humidity, uint32_t *pressure);
void    bme280_sleep(void);
void    bme280_wakeup(void);

#endif
