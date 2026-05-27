/**
 * @file lv_port_disp.c
 * @brief  LVGL 9.x 显示接口 —— ST7789 240x280 RGB565
 *
 * @par DMA + 双缓冲 架构说明
 * ───────────────────────────────
 *
 * 【为什么要 DMA】
 *   原来的 HAL_SPI_Transmit 是阻塞的：CPU 搬一个字节到 SPI 寄存器，等它发完，
 *   再搬下一个。一帧 240×280×2=131KB，CPU 全程被占着等 SPI。
 *   DMA（直接内存访问）是一个独立的硬件控制器，CPU 告诉它"从这地址取 N 个字节
 *   发到 SPI"，DMA 就在后台自动搬运，CPU 同时可以干别的事（执行 LVGL 渲染、
 *   处理触摸）。
 *
 * 【为什么要双缓冲】
 *   LVGL 用 PARTIAL 模式，每次只渲染脏区域（最多 20 行×240 像素=9.6KB）。
 *   单缓冲时：LVGL 渲染到 buf → disp_flush → DMA 发 → CPU 傻等 DMA 完成 →
 *   flush_ready 释放 buf → LVGL 渲染下一块。
 *
 *   双缓冲时：LVGL 有两块等大的 buf_1 和 buf_2。LVGL 往里填 buf_1，填完交给
 *   DMA 发送；同时 LVGL 发现 buf_2 还空着，直接往 buf_2 填下一块。两块轮流用，
 *   CPU 渲染和 DMA 发送并行进行（乒乓）。
 *
 *   LVGL 内部自己管理 buffer 轮换：哪个 buf 的 flush_ready 还没调，哪个就标记
 *   为"使用中"，LVGL 不会碰。我们推迟 flush_ready 到 DMA 中断里调，让 buf 在
 *   DMA 发送期间受保护。
 *
 * 【数据流】
 *   ┌─────────────┐     px_map       ┌──────────────┐
 *   │ LVGL 渲染到  │ ──────────────→ │  disp_flush   │
 *   │ buf_1/buf_2 │                 │ 启动 DMA 发送  │
 *   └─────────────┘                 └──────┬─────────┘
 *                                          │ 异步，立即返回
 *   ┌─────────────┐                 ┌──────▼─────────┐
 *   │ LVGL 继续用  │                │ DMA 后台搬运    │
 *   │ 另一个 buf   │                │ px_map → SPI    │
 *   └─────────────┘                 └──────┬─────────┘
 *                                          │ 完成后中断
 *                                 ┌────────▼──────────┐
 *                                 │ DMA 中断回调       │
 *                                 │ → CS 拉高          │
 *                                 │ → flush_ready     │
 *                                 │ → 释放该 buf      │
 *                                 └───────────────────┘
 *
 * 【dma_busy 保护】
 *   DMA 硬件只能同时跑一个传输。两块 LVGL buffer 轮流调 disp_flush，但中间
 *   只隔 5ms，上一个 DMA 可能还没发完。如果此时再调 HAL_SPI_Transmit_DMA，
 *   HAL 返回 HAL_BUSY 错误，数据丢失、画面花屏。
 *   dma_busy 在启动 DMA 前置 true，DMA 中断回调里置 false，disp_flush 入口
 *   忙等。保证同一时刻最多一个 DMA 传输在跑。
 *
 * @par 内存账本（STM32F411CE, 128KB SRAM）
 *   buf_1 + buf_2 = 240×35×2 ×2 = 32.8 KB
 */

 /*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "lcd_st7789.h"

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    280

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))

/**********************
 *   STATIC VARIABLES
 **********************/
static lv_display_t * my_disp = NULL;   /* 全局句柄，供 DMA 中断回调取用 */

/* DMA 互斥锁：true=SPI 正在被 DMA 占用，新 flush 需等待 */
static volatile bool dma_busy = false;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    disp_init();

    /* 创建 LVGL 显示对象，注册 flush 回调 */
    my_disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_color_format(my_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(my_disp, disp_flush);

    /* 双缓冲：buf_1 和 buf_2 各 35 行≈16.4KB，满屏 8 次 DMA */
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_1[MY_DISP_HOR_RES * 35 * BYTE_PER_PIXEL];
    static uint8_t buf_2[MY_DISP_HOR_RES * 35 * BYTE_PER_PIXEL];
    lv_display_set_buffers(my_disp, buf_1, buf_2, sizeof(buf_1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void disp_init(void)
{
    ST7789_Init();
}

/**
 * @brief  LVGL flush 回调 — 异步 DMA 版
 * @note   关键：不在此函数内调 lv_display_flush_ready。
 *         回调被推迟到 DMA 发送完成中断（HAL_SPI_TxCpltCallback），
 *         这样 LVGL 认为该 buf 仍在使用，不会覆盖，转而使用另一块 buf。
 *
 *         同时 dma_busy 保证 SPI 外设不被重入。
 *         px_map 直接传 DMA，无需 memcpy 到中间缓冲区 —— 因为 LVGL 在
 *         flush_ready 之前不会碰这块 buf，数据是 DMA-安全的。
 */
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    while (dma_busy) {}   /* 等上一次 DMA 传输完成 */
    dma_busy = true;      /* 锁住 SPI 外设 */

    ST7789_FlushArea_DMA(area->x1, area->y1, w, h, px_map);
    /* flush_ready 不在这里 — 见 HAL_SPI_TxCpltCallback */
}

/**********************
 *   GLOBAL FUNCTIONS (exported for BSP)
 **********************/

/**
 * @brief  供 DMA 中断回调调用，释放 LVGL buffer
 * @note   先解锁 dma_busy，再调 lv_display_flush_ready。
 *         顺序重要：先通知 SPI 硬件空闲，再通知 LVGL 软件层该 buf 可复用。
 */
void lv_port_disp_flush_ready(void)
{
    dma_busy = false;                      /* 解锁 SPI 硬件 */
    lv_display_flush_ready(my_disp);       /* 通知 LVGL：当前 buf 已空闲 */
}

#else

typedef int keep_pedantic_happy;
#endif
