/**
 * @file    max30102.c
 * @brief   MAX30102 心率血氧 — 原始 I2C 收发 (不用 Mem 函数)
 */

#include "max30102.h"
#include "i2c.h"

/* raw write: [DEV+W][REG][DATA], 等同于 Demo 的 i2c.write(addr, buf, 2) */
static uint8_t raw_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = data;
    HAL_StatusTypeDef rc;
    rc = HAL_I2C_Master_Transmit(&hi2c1, MAX30102_ADDR_8BIT, buf, 2, 100);
    return (rc == HAL_OK) ? 0 : 1;
}

/* raw read: [DEV+W][REG] + [DEV+R][DATA...], 等同于 Demo 的 write+read */
static uint8_t raw_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    /* 先发寄存器地址 */
    if (HAL_I2C_Master_Transmit(&hi2c1, MAX30102_ADDR_8BIT, &reg, 1, 100) != HAL_OK)
        return 1;
    /* 再读数据 */
    if (HAL_I2C_Master_Receive(&hi2c1, MAX30102_ADDR_8BIT, buf, len, 100) != HAL_OK)
        return 1;
    return 0;
}

uint8_t max30102_init(void)
{
    uint8_t dummy;

    /* ① 复位 */
    raw_write(0x09, 0x40);
    HAL_Delay(10);

    /* ② 清状态 (Demo line 92) */
    raw_read(0x00, &dummy, 1);

    /* ③ Demo 标准 init */
    raw_write(0x02, 0xC0);  /* INTR_ENABLE_1 */
    raw_write(0x03, 0x00);  /* INTR_ENABLE_2 */
    raw_write(0x04, 0x00);  /* FIFO_WR_PTR   */
    raw_write(0x05, 0x00);  /* OVF_COUNTER   */
    raw_write(0x06, 0x00);  /* FIFO_RD_PTR   */
    raw_write(0x08, 0x0F);  /* FIFO_CONFIG   */
    raw_write(0x09, 0x03);  /* MODE: SPO2    */
    raw_write(0x0A, 0x27);  /* SPO2_CONFIG   */
    raw_write(0x0C, 0x24);  /* LED1_PA ~7mA  */
    raw_write(0x0D, 0x24);  /* LED2_PA ~7mA  */
    raw_write(0x10, 0x7F);  /* PILOT_PA      */

    HAL_Delay(100);

    /* 验证: 读 PART_ID + MODE */
    {
        uint8_t id, mode;
        raw_read(0xFF, &id, 1);
        raw_read(0x09, &mode, 1);
        /* PART_ID 应为 0x15, MODE 应为 0x03 */
        if (id != 0x15)  return 10;
        if (mode != 0x03) return 11;
    }

    return 0;
}

uint8_t max30102_read(uint32_t *red, uint32_t *ir)
{
    uint8_t data[6], wr, rd;

    /* 有数据才读 */
    raw_read(0x04, &wr, 1);
    raw_read(0x06, &rd, 1);
    if (wr == rd) { *red = 0; *ir = 0; return 1; }

    /* 清中断 */
    raw_read(0x00, data, 1);
    raw_read(0x01, data, 1);

    /* 读 FIFO */
    raw_read(0x07, data, 6);

    *red = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    *ir  = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
    *red &= 0x03FFFF;
    *ir  &= 0x03FFFF;

    return 0;
}

void max30102_sleep(void) { raw_write(0x09, 0x80); }
void max30102_wakeup(void) { raw_write(0x09, 0x03); }
