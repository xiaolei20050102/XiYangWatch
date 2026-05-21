/**
 * @file    lcd_st7789.h
 * @author  xiyangdaxia
 * @brief   ST7789 LCD 驱动（240x280），硬件抽象层集中于此文件
 * @date    2026-5-20
 * @version v1.0
 *
 * @details 移植到其他平台时，只需修改本文件中"硬件抽象宏"区域的宏定义
 *
 * @note    OFFSET_Y=20 是模组 bonding 偏移，不同模组可能不同
 *
 * @par 修改记录:
 * - 2026-05-20  v1.0  创建文件
 * - 2026-05-21  修复 SPI 引脚、RST 引脚、OFFSET_Y，抽象 HAL 调用
 */

#ifndef __LEC_ST7789_H__
#define __LEC_ST7789_H__

/* ========================== 头文件包含 ========================== */
#include "spi.h"

/* ========================== 引脚 & 外设定义 ========================== */

#define LCD_PIN_CS    GPIO_PIN_1
#define LCD_PORT_CS   GPIOB
#define LCD_PIN_DC    GPIO_PIN_0
#define LCD_PORT_DC   GPIOB
#define LCD_PIN_RST   GPIO_PIN_12
#define LCD_PORT_RST  GPIOB
#define LCD_SPI       &hspi2

/* ========================== 面板参数 ========================== */

#define LCD_WIDTH   240
#define LCD_HEIGHT  280
#define OFFSET_Y    20    /* 面板可见区从 GRAM 第20行开始（模组 bonding 偏移） */

/* ========================== 硬件抽象宏（移植时只改这里） ========================== */

/* 引脚操作 */
#define LCD_CS_LOW()    HAL_GPIO_WritePin(LCD_PORT_CS,  LCD_PIN_CS,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(LCD_PORT_CS,  LCD_PIN_CS,  GPIO_PIN_SET)
#define LCD_DC_LOW()    HAL_GPIO_WritePin(LCD_PORT_DC,  LCD_PIN_DC,  GPIO_PIN_RESET)
#define LCD_DC_HIGH()   HAL_GPIO_WritePin(LCD_PORT_DC,  LCD_PIN_DC,  GPIO_PIN_SET)
#define LCD_RST_LOW()   HAL_GPIO_WritePin(LCD_PORT_RST, LCD_PIN_RST, GPIO_PIN_RESET)
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(LCD_PORT_RST, LCD_PIN_RST, GPIO_PIN_SET)

/* SPI 发送 */
#define LCD_SPI_TX(pData, Size)  HAL_SPI_Transmit(LCD_SPI, (pData), (Size), 10)

/* 毫秒延时 */
#define LCD_DELAY_MS(ms)  HAL_Delay(ms)

/* ========================== 常用颜色 ========================== */

#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

/* ========================== 函数声明 ========================== */

void ST7789_Init(void);
void ST7789_SendCmd(uint8_t cmd, const uint8_t* data, uint16_t len);
void ST7789_Fill(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void ST7789_Test(void);

#endif
