/**
 * @file    lcd_st7789.c
 * @author  xiyangdaxia
 * @brief   ST7789 LCD 驱动实现（240x280 面板，SPI2 Mode 0，RGB565）
 * @date    2026-05-21
 * @version v1.2
 *
 * @details 本文件不直接调用 HAL，所有硬件操作通过 lcd_st7789.h 中的宏完成
 *          初始化序列参考 OV-Watch v2.4.3，经验证可正常工作
 *
 * @note    更换模组需确认两个参数：OFFSET_Y（bonding 偏移）、SPI 模式（Mode 0/3）
 *
 * @par 修改记录:
 * - 2026-05-20  v1.0  创建文件，实现 ST7789 基本初始化和全屏填充
 * - 2026-05-21  v1.1  修复 SPI 引脚(hspi1→hspi2)、RST 引脚(PB2→PB12)、增加 OFFSET_Y 偏移
 * - 2026-05-21  v1.2  抽象 HAL 调用为宏，新增 DrawPixel/DrawLine/DrawCircle，优化 DrawRect 行缓冲，完善自检流程
 */

#include "lcd_st7789.h"
#include "lv_port_disp.h"
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
 * @brief   绘制单个像素点
 * @param   x      [入] 列坐标（0 ~ LCD_WIDTH-1）
 * @param   y      [入] 行坐标（0 ~ LCD_HEIGHT-1）
 * @param   color  [入] 16-bit RGB565 颜色值
 * @retval  无
 * @note    单点发送：CASET(1列) + RASET(1行) + RAMWR(2字节)
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint16_t gy = y + OFFSET_Y;

    { uint8_t p[] = {x>>8, x&0xFF, x>>8, x&0xFF}; ST7789_SendCmd(0x2A, p, 4); }
    { uint8_t p[] = {gy>>8, gy&0xFF, gy>>8, gy&0xFF}; ST7789_SendCmd(0x2B, p, 4); }

    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t ramwr = 0x2C;
    LCD_SPI_TX(&ramwr, 1);
    LCD_DC_HIGH();
    uint8_t p[2] = {(color >> 8) & 0xFF, color & 0xFF};
    LCD_SPI_TX(p, 2);
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
 * @note    使用行缓冲，每行一次 SPI 发送（w×2 字节），比逐像素快 w 倍
 */
void ST7789_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;

    uint16_t xs = x, xe = x + w - 1;
    uint16_t ys = y + OFFSET_Y, ye = y + OFFSET_Y + h - 1;

    { uint8_t p[] = {xs>>8, xs&0xFF, xe>>8, xe&0xFF}; ST7789_SendCmd(0x2A, p, 4); }
    { uint8_t p[] = {ys>>8, ys&0xFF, ye>>8, ye&0xFF}; ST7789_SendCmd(0x2B, p, 4); }

    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t ramwr = 0x2C;
    LCD_SPI_TX(&ramwr, 1);
    LCD_DC_HIGH();

    /* 行缓冲：一次 SPI 发一整行 */
    uint8_t line_buf[w * 2];
    for (uint16_t i = 0; i < w; i++) {
        line_buf[i * 2]     = hi;
        line_buf[i * 2 + 1] = lo;
    }
    for (uint16_t row = 0; row < h; row++) {
        LCD_SPI_TX(line_buf, sizeof(line_buf));
    }

    LCD_CS_HIGH();
}

/**
 * @brief   Bresenham 画线算法
 * @param   x1     [入] 起点列坐标
 * @param   y1     [入] 起点行坐标
 * @param   x2     [入] 终点列坐标
 * @param   y2     [入] 终点行坐标
 * @param   color  [入] 16-bit RGB565 颜色值
 * @retval  无
 * @note    经典整数 Bresenham，无浮点运算
 */
void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = (int16_t)x2 - (int16_t)x1;
    int16_t dy = (int16_t)y2 - (int16_t)y1;
    int16_t stepX = (dx > 0) ? 1 : -1;
    int16_t stepY = (dy > 0) ? 1 : -1;
    dx = (dx > 0) ? dx : -dx;
    dy = (dy > 0) ? dy : -dy;

    int16_t x = x1, y = y1;
    ST7789_DrawPixel(x, y, color);

    if (dx > dy) {
        int16_t err = dx / 2;
        for (int16_t i = 0; i < dx; i++) {
            x += stepX;
            err -= dy;
            if (err < 0) { y += stepY; err += dx; }
            ST7789_DrawPixel(x, y, color);
        }
    } else {
        int16_t err = dy / 2;
        for (int16_t i = 0; i < dy; i++) {
            y += stepY;
            err -= dx;
            if (err < 0) { x += stepX; err += dy; }
            ST7789_DrawPixel(x, y, color);
        }
    }
}

/**
 * @brief   Midpoint 画圆算法
 * @param   x0     [入] 圆心列坐标
 * @param   y0     [入] 圆心行坐标
 * @param   r      [入] 半径（像素）
 * @param   color  [入] 16-bit RGB565 颜色值
 * @retval  无
 * @note    利用八对称性一次计算 8 个点，经典整数算法
 */
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 1 - r;

    while (x <= y) {
        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 + y, y0 + x, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 - y, y0 + x, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        ST7789_DrawPixel(x0 + y, y0 - x, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 - y, y0 - x, color);

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/**
 * @brief  LCD 全面自检：纯色 → 条纹 → 边框 → 线条/圆 → 棋盘格
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
    ST7789_DrawRect(0, 0, sw, 2, COLOR_WHITE);
    ST7789_DrawRect(0, sh - 2, sw, 2, COLOR_WHITE);
    ST7789_DrawRect(0, 0, 2, sh, COLOR_WHITE);
    ST7789_DrawRect(sw - 2, 0, 2, sh, COLOR_WHITE);
    LCD_DELAY_MS(800);

    /* 5. 线条测试：对角线 × 2 + 中心十字（验证 DrawLine） */
    ST7789_Fill(COLOR_BLACK);
    ST7789_DrawLine(0, 0, sw - 1, sh - 1, COLOR_GREEN);       // 主对角线
    ST7789_DrawLine(sw - 1, 0, 0, sh - 1, COLOR_CYAN);        // 副对角线
    ST7789_DrawLine(sw / 2, 0, sw / 2, sh - 1, COLOR_RED);    // 垂直中线
    ST7789_DrawLine(0, sh / 2, sw - 1, sh / 2, COLOR_RED);    // 水平中线
    LCD_DELAY_MS(800);

    /* 6. 圆形测试：4 个同心圆（验证 DrawCircle） */
    ST7789_Fill(COLOR_BLACK);
    uint16_t cx = sw / 2, cy = sh / 2;
    ST7789_DrawCircle(cx, cy, 20, COLOR_RED);
    ST7789_DrawCircle(cx, cy, 40, COLOR_GREEN);
    ST7789_DrawCircle(cx, cy, 60, COLOR_BLUE);
    ST7789_DrawCircle(cx, cy, 80, COLOR_WHITE);
    LCD_DELAY_MS(800);

    /* 7. 棋盘格 20×20（验证像素级寻址无错位） */
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

    /* 8. 结束画面：品红色 = 自检通过 */
    ST7789_Fill(COLOR_MAGENTA);
}

/**
 * @brief   向指定区域写入像素数据（供 LVGL disp_flush 调用）
 * @param   x      [入] 起始列坐标（0 ~ LCD_WIDTH-1，LCD 坐标系）
 * @param   y      [入] 起始行坐标（0 ~ LCD_HEIGHT-1，LCD 坐标系）
 * @param   w      [入] 区域宽度（像素）
 * @param   h      [入] 区域高度（像素）
 * @param   data   [入] 16-bit RGB565 像素数据缓冲区
 * @retval  无
 * @note    自动叠加 OFFSET_Y 偏移；CS 全区域保持低，一次 SPI 连续发送提高效率
 */
void ST7789_FlushArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data)
{
	uint16_t xs = x, xe = x + w - 1;
	uint16_t ys = y + OFFSET_Y, ye = y + OFFSET_Y + h - 1;

	{ uint8_t p[] = {xs>>8, xs&0xFF, xe>>8, xe&0xFF}; ST7789_SendCmd(0x2A, p, 4); }
	{ uint8_t p[] = {ys>>8, ys&0xFF, ye>>8, ye&0xFF}; ST7789_SendCmd(0x2B, p, 4); }

	LCD_CS_LOW();
	LCD_DC_LOW();
	uint8_t ramwr = 0x2C;
	LCD_SPI_TX(&ramwr, 1);
	LCD_DC_HIGH();

	LCD_SPI_TX((uint8_t*)data, (uint32_t)w * h * 2);

	LCD_CS_HIGH();
}

/**
 * @brief   设置 LCD 背光亮度（TIM2_CH2 PWM）
 * @param   percent  [入] 亮度百分比 0~100（0=灭, 100=最亮）
 * @retval  无
 * @note    首次调用自动启动 PWM，之后只调占空比
 *          PWM 分辨率：Period=99，实际占空比 = percent * 99 / 100
 */
void ST7789_SetBacklight(uint8_t percent)
{
    static uint8_t started = 0;

    if (percent > 100) percent = 100;

    if (!started) {
        HAL_TIM_PWM_Start(LCD_BL_TIM, LCD_BL_CH);
        started = 1;
    }

    uint16_t pulse = (uint16_t)percent * LCD_BL_MAX / 100;
    __HAL_TIM_SET_COMPARE(LCD_BL_TIM, LCD_BL_CH, pulse);
}


/* ═══════════════ DMA 异步刷新接口 ═══════════════
 *
 * 以下两个函数替代了原来的 ST7789_FlushArea 阻塞式刷新。
 *
 * 【设计思路】
 *   ST7789_FlushArea_DMA: CASET/RASET/RAMWR 命令用阻塞 SPI 发出（命令必须
 *   保证时序），像素数据用 HAL_SPI_Transmit_DMA 异步发送。CS 保持低（函数
 *   不拉高），让 ST7789 知道像素流还在进行中。
 *
 *   ST7789_FlushComplete: DMA 传输完成后由 HAL_SPI_TxCpltCallback 调用，
 *   拉高 CS 结束本次传输，保护 SPI 总线不被意外片选干扰。
 *
 *   HAL_SPI_TxCpltCallback: HAL 的 DMA 完成回调（覆盖了 __weak 默认实现）。
 *   在 SPI2 的 DMA 发送完成后：拉 CS → 通知 LVGL 释放 buffer。
 *
 * 【为什么 CS 在中断里拉高】
 *   如果 DMA 还在发数据就把 CS 拉高，ST7789 认为传输结束，后半截像素丢失 →
 *   画面撕裂。必须等 DMA 完全发完（传输完成中断）才 CS↑。
 */
void ST7789_FlushArea_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data) {
    uint16_t xs = x, xe = x + w - 1;
    uint16_t ys = y + OFFSET_Y, ye = y + OFFSET_Y + h - 1;

    /* CASET + RASET 设定写入窗口（阻塞发送，命令不能异步） */
    { uint8_t p[] = {xs>>8, xs&0xFF, xe>>8, xe&0xFF}; ST7789_SendCmd(0x2A, p, 4); }
    { uint8_t p[] = {ys>>8, ys&0xFF, ye>>8, ye&0xFF}; ST7789_SendCmd(0x2B, p, 4); }

    LCD_CS_LOW();
    LCD_DC_LOW();
    uint8_t ramwr = 0x2C;
    LCD_SPI_TX(&ramwr, 1);      /* 发送 RAMWR 命令（1字节，阻塞） */
    LCD_DC_HIGH();               /* 之后全是像素数据 */

    /* 像素数据用 DMA 异步发送，函数立即返回，CS 保持低 */
    HAL_SPI_Transmit_DMA(LCD_SPI, (uint8_t*)data, (uint32_t)w * h * 2);
}

void ST7789_FlushComplete(void) {
    LCD_CS_HIGH();  /* DMA 发完，拉高 CS 保护总线 */
}

/* 覆盖 HAL 的 __weak HAL_SPI_TxCpltCallback */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (LCD_SPI == hspi) {              /* 只处理 SPI2 的传输完成 */
        ST7789_FlushComplete();         /* CS↑ 结束写屏 */
        lv_port_disp_flush_ready();     /* 通知 LVGL 释放 buffer */
    }
}