/**
 * @file    w25q64_port.c
 * @brief   W25Q64 STM32F411 移植层，将 HAL 接口注入公共驱动
 * @note    SPI1 — PA5(SCK) PA6(MISO) PA7(MOSI), CS — PA4(GPIO)
 */

#include "w25q64_port.h"
#include "w25q64.h"
#include "main.h"
#include "spi.h"
#include "lvgl.h"

/* ═══════════════════════════════════════════════════════════════
 * HAL 适配回调
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  SPI 收发：通过 SPI1 完成命令/数据交互
 */
static void port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    if (tx && rx) {
        HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, HAL_MAX_DELAY);
    } else if (tx) {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, HAL_MAX_DELAY);
    } else if (rx) {
        HAL_SPI_Receive(&hspi1, rx, len, HAL_MAX_DELAY);
    }
}

/**
 * @brief  CS 控制：enable=true 拉低片选，enable=false 拉高
 */
static void port_cs_ctrl(bool enable)
{
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief  毫秒延时
 */
static void port_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief  系统滴答，返回启动以来的毫秒数
 */
static uint32_t port_tick(void)
{
    return lv_tick_get();
}

/* ═══════════════════════════════════════════════════════════════
 * 初始化入口
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化 W25Q64，将 HAL 函数指针注入公共驱动
 * @retval true 芯片识别成功 / false 通信失败或 ID 不匹配
 */
bool w25q64_port_init(void)
{
    /* 确保 CS 初始为高（不选中） */
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_SET);

    return w25q64_init(port_spi_xfer, port_cs_ctrl, port_delay_ms, port_tick);
}
