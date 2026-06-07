#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>

/* MAX30102 心率血氧传感器 — BSP 层纯驱动
 *
 * I2C 地址: 0x57
 * 总线:     I2C1
 * 周期:     每 20 周期 (100ms)
 * 输出:     red / ir 原始值 → 上层做心率算法
 */

#define MAX30102_ADDR       0x57
#define MAX30102_ADDR_8BIT  ((MAX30102_ADDR) << 1)
#define MAX30102_I2C        &hi2c1

#define MAX30102_I2C_WRITE(reg, pData, sz) \
    HAL_I2C_Mem_Write(MAX30102_I2C, MAX30102_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define MAX30102_I2C_READ(reg, pData, sz) \
    HAL_I2C_Mem_Read(MAX30102_I2C, MAX30102_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

/* ========================== 寄存器 ========================== */
#define MAX30102_REG_INT_STS1   0x00
#define MAX30102_REG_INT_STS2   0x01
#define MAX30102_REG_FIFO_WR    0x04
#define MAX30102_REG_FIFO_RD    0x06
#define MAX30102_REG_FIFO_DATA  0x07
#define MAX30102_REG_MODE_CFG   0x09
#define MAX30102_REG_SPO2_CFG   0x0A
#define MAX30102_REG_LED1_PA    0x0C  /* Red LED */
#define MAX30102_REG_LED2_PA    0x0D  /* IR LED  */
#define MAX30102_REG_TEMP_INT   0x1F
#define MAX30102_REG_TEMP_FRAC  0x20
#define MAX30102_REG_PART_ID    0xFF  /* 应返回 0x15 */

/* ========================== API ========================== */

uint8_t max30102_init(void);
uint8_t max30102_read(uint32_t *red, uint32_t *ir);
void    max30102_sleep(void);
void    max30102_wakeup(void);

#endif
