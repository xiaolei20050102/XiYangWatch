/**
 * @file lv_port_disp.c
 * @brief  LVGL 9.x 显示接口 —— ST7789 240x280 RGB565
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

    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 10 行部分缓冲 ≈ 4.8 KB，适合 STM32F411 128KB SRAM */
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void disp_init(void)
{
    ST7789_Init();
}

static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    ST7789_FlushArea(area->x1, area->y1, w, h, px_map);

    lv_display_flush_ready(disp_drv);
}

#else

typedef int keep_pedantic_happy;
#endif
