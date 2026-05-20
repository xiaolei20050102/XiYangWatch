/**
 * @file    lcd_st7789.c
 * @author  xiyangdaxia
 * @brief   ST7789 LCD 驱动实现（240x280 面板，SPI2，Mode 0）
 * @date    2026-5-20
 * @version v1.0
 *
 * @details 硬件抽象通过 lcd_st7789.h 中的宏完成，本文件不直接调用 HAL。
 *          初始化序列参考 OV-Watch v2.4.3，已验证通过。
 *
 * @note    OFFSET_Y = 20 是本模组的 bonding 偏移，更换模组需确认此值
 *
 * @par 修改记录:
 * - 2025-01-15  v1.0  创建文件，实现基本初始化
 * - 2026-05-20  修复 SPI/RST 引脚错误，增加 OFFSET_Y，抽象 HAL 调用
 */

#include "lcd_st7789.h"

/* ========================== 全局函数实现 ========================== */

/**
 * @brief  向 ST7789 发送命令字节 + 可选数据字节
 * @param  cmd   [入] 8-bit 命令码
 * @param  data  [入] 数据缓冲区指针，无数据时传 NULL
 * @param  len   [入] 数据长度（字节），无数据时传 0
 * @retval  无
 * @note    CS 在函数内自动拉低/拉高；DC 由函数根据数据有无自动切换
 */
void ST7789_SendCmd(uint8_t cmd, const uint8_t* data, uint16_t len)
{
    LCD_CS_LOW();
    LCD_DC_LOW();
    LCD_SPI_TX(&cmd, 1);

    if (len > 0 && data != NULL) {
        LCD_DC_HIGH();
        LCD_SPI_TX((uint8_t*)data, len);
    }

    LCD_CS_HIGH();
}

/**
 * @brief  ST7789 完整初始化序列
 * @param   无
 * @retval  无
 * @note    包含硬件复位 → SLPOUT → MADCTL → COLMOD → 时序/电源/Gamma → INVON → DISPON
 *          复位后等待 100ms+100ms，SLPOUT 后等待 120ms
 */
void ST7789_Init(void)
{
    /* === 硬件复位 === */
    LCD_RST_LOW();
    LCD_DELAY_MS(100);
    LCD_RST_HIGH();
    LCD_DELAY_MS(100);

    /* === 退出休眠 === */
    ST7789_SendCmd(0x11, NULL, 0);   // SLPOUT
    LCD_DELAY_MS(120);

    /* === 显示方向 === */
    { uint8_t p = 0x00; ST7789_SendCmd(0x36, &p, 1); }  // MADCTL

    /* === 颜色格式：16-bit RGB565 === */
    { uint8_t p = 0x05; ST7789_SendCmd(0x3A, &p, 1); }  // COLMOD

    /* === 前后沿时序 === */
    { uint8_t p[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; ST7789_SendCmd(0xB2, p, 5); }

    /* === 栅极控制 === */
    { uint8_t p = 0x35; ST7789_SendCmd(0xB7, &p, 1); }

    /* === VCOM 电压 === */
    { uint8_t p = 0x19; ST7789_SendCmd(0xBB, &p, 1); }

    /* === 电源控制 === */
    { uint8_t p = 0x2C; ST7789_SendCmd(0xC0, &p, 1); }
    { uint8_t p = 0x01; ST7789_SendCmd(0xC2, &p, 1); }
    { uint8_t p = 0x12; ST7789_SendCmd(0xC3, &p, 1); }
    { uint8_t p = 0x20; ST7789_SendCmd(0xC4, &p, 1); }

    /* === 帧率控制 60Hz === */
    { uint8_t p = 0x0F; ST7789_SendCmd(0xC6, &p, 1); }

    /* === NV 存储器 === */
    { uint8_t p[] = {0xA4, 0xA1}; ST7789_SendCmd(0xD0, p, 2); }

    /* === Gamma 正曲线 === */
    { uint8_t p[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
      ST7789_SendCmd(0xE0, p, 14); }

    /* === Gamma 负曲线 === */
    { uint8_t p[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
      ST7789_SendCmd(0xE1, p, 14); }

    /* === 开屏 === */
    ST7789_SendCmd(0x21, NULL, 0);   // INVON
    ST7789_SendCmd(0x29, NULL, 0);   // DISPON
}

/**
 * @brief   全屏填充纯色
 * @param   color  [入] 16-bit RGB565 颜色值
 * @retval  无
 * @note    先设 CASET(0~239) + RASET(20~299)，再逐行 SPI 发送像素数据
 *          每行 480 字节一次发送，共 280 行，比逐像素发送快 240 倍
 */
void ST7789_Fill(uint16_t color)
{
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;

    /* 设置列范围 0~239 */
    { uint8_t p[] = {0x00, 0x00, 0x00, 0xEF}; ST7789_SendCmd(0x2A, p, 4); }

    /* 设置行范围 OFFSET_Y ~ OFFSET_Y+LCD_HEIGHT-1 (20~299) */
    { uint16_t sr = OFFSET_Y, er = OFFSET_Y + LCD_HEIGHT - 1;
      uint8_t p[] = {sr>>8, sr&0xFF, er>>8, er&0xFF};
      ST7789_SendCmd(0x2B, p, 4); }

    /* 写 RAMWR + 整屏数据，全程 CS 保持低 */
    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t ramwr = 0x2C;
    LCD_SPI_TX(&ramwr, 1);
    LCD_DC_HIGH();

    /* 逐行发送，每行 240 像素 × 2 字节 = 480 字节 */
    uint8_t line_buf[LCD_WIDTH * 2];
    for (uint16_t i = 0; i < LCD_WIDTH; i++) {
        line_buf[i * 2]     = hi;
        line_buf[i * 2 + 1] = lo;
    }
    for (uint16_t row = 0; row < LCD_HEIGHT; row++) {
        LCD_SPI_TX(line_buf, sizeof(line_buf));
    }

    LCD_CS_HIGH();
}

/**
 * @brief   在指定区域填充纯色（LCD 坐标系，自动叠加 OFFSET_Y）
 * @param   x      [入] 起始列坐标（0 ~ LCD_WIDTH-1）
 * @param   y      [入] 起始行坐标（0 ~ LCD_HEIGHT-1）
 * @param   w      [入] 区域宽度（像素）
 * @param   h      [入] 区域高度（像素）
 * @param   color  [入] 16-bit RGB565 颜色值
 * @retval  无
 * @note    自动将 LCD 坐标转换为 GRAM 坐标（y += OFFSET_Y）
 */
void ST7789_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;

    uint16_t xs = x, xe = x + w - 1;
    uint16_t ys = y + OFFSET_Y, ye = y + OFFSET_Y + h - 1;

    /* CASET */
    { uint8_t p[] = {xs>>8, xs&0xFF, xe>>8, xe&0xFF}; ST7789_SendCmd(0x2A, p, 4); }
    /* RASET */
    { uint8_t p[] = {ys>>8, ys&0xFF, ye>>8, ye&0xFF}; ST7789_SendCmd(0x2B, p, 4); }

    /* RAMWR + 数据 */
    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t ramwr = 0x2C;
    LCD_SPI_TX(&ramwr, 1);
    LCD_DC_HIGH();

    uint32_t total = (uint32_t)w * h;
    for (uint32_t i = 0; i < total; i++) {
        uint8_t p[2] = {hi, lo};
        LCD_SPI_TX(p, 2);
    }

    LCD_CS_HIGH();
}

/**
 * @brief  LCD 全面自检：纯色填充 → 彩色条纹 → 边框 → 十字线 → 棋盘格
 * @param   无
 * @retval  无
 * @note    每个测试项停留约 800ms，最后停在品红色表示自检通过
 *          若某一步颜色不对或花屏，可快速定位是哪个通道或区域有问题
 */
void ST7789_Test(void)
{
    uint16_t sw = LCD_WIDTH;
    uint16_t sh = LCD_HEIGHT;

    /* 1. RGB 三通道纯色验证 */
    ST7789_Fill(COLOR_RED);     LCD_DELAY_MS(500);
    ST7789_Fill(COLOR_GREEN);   LCD_DELAY_MS(500);
    ST7789_Fill(COLOR_BLUE);    LCD_DELAY_MS(500);

    /* 2. 黑白验证 */
    ST7789_Fill(COLOR_WHITE);   LCD_DELAY_MS(500);
    ST7789_Fill(COLOR_BLACK);   LCD_DELAY_MS(500);

    /* 3. 彩色竖条纹：R | G | B | W | Black（验证水平寻址） */
    uint16_t bw = sw / 5;
    ST7789_DrawRect(bw * 0, 0, bw, sh, COLOR_RED);
    ST7789_DrawRect(bw * 1, 0, bw, sh, COLOR_GREEN);
    ST7789_DrawRect(bw * 2, 0, bw, sh, COLOR_BLUE);
    ST7789_DrawRect(bw * 3, 0, bw, sh, COLOR_WHITE);
    ST7789_DrawRect(bw * 4, 0, sw - bw * 4, sh, COLOR_BLACK);
    LCD_DELAY_MS(800);

    /* 4. 屏幕四边 2px 白框（验证边界寻址） */
    ST7789_DrawRect(0, 0, sw, 2, COLOR_WHITE);          // 顶边
    ST7789_DrawRect(0, sh - 2, sw, 2, COLOR_WHITE);     // 底边
    ST7789_DrawRect(0, 0, 2, sh, COLOR_WHITE);          // 左边
    ST7789_DrawRect(sw - 2, 0, 2, sh, COLOR_WHITE);     // 右边
    LCD_DELAY_MS(800);

    /* 5. 中心十字线（验证横纵中点） */
    ST7789_DrawRect(0, sh / 2 - 1, sw, 2, COLOR_RED);   // 水平中线
    ST7789_DrawRect(sw / 2 - 1, 0, 2, sh, COLOR_RED);   // 垂直中线
    LCD_DELAY_MS(800);

    /* 6. 棋盘格 20×20（验证像素级寻址无错位） */
    uint16_t cs = 20;
    for (uint16_t row = 0; row < sh; row += cs) {
        for (uint16_t col = 0; col < sw; col += cs) {
            uint16_t c = ((row / cs) + (col / cs)) % 2 ? COLOR_WHITE : COLOR_BLACK;
            uint16_t cw = (col + cs > sw) ? sw - col : cs;
            uint16_t ch = (row + cs > sh) ? sh - row : cs;
            ST7789_DrawRect(col, row, cw, ch, c);
        }
    }
    LCD_DELAY_MS(800);

    /* 7. 结束画面：品红色 = 自检通过 */
    ST7789_Fill(COLOR_MAGENTA);
}
