/**
 * @file    w25q64_program.c
 * @brief   W25Q64 烧录工具实现
 * @note    一次性将壁纸资源写入外部 Flash，烧录完成后可从构建中移除
 */

#include "w25q64_program.h"
#include "w25q64.h"

/* 壁纸 raw 像素数据来自旧版 wallpaper.c */
extern const unsigned char wallpaper_map[];

#define WALLPAPER_W      240U
#define WALLPAPER_H      280U
#define WALLPAPER_STRIDE (WALLPAPER_W * 2U)
#define WALLPAPER_ADDR   0x00000000U
#define HEADER_SIZE      12U

/**
 * @brief  写入 LVGL image header 到 W25Q64
 */
static bool write_header(void)
{
    uint8_t hdr[HEADER_SIZE];

    /* lv_image_header_t (little-endian bit-fields): 12 bytes */
    hdr[0]  = 0x19;  /* magic  = LV_IMAGE_HEADER_MAGIC */
    hdr[1]  = 0x12;  /* cf     = LV_COLOR_FORMAT_RGB565 */
    hdr[2]  = 0x00;  /* flags (low)  */
    hdr[3]  = 0x00;  /* flags (high) */
    hdr[4]  = (uint8_t)(WALLPAPER_W & 0xFF);       /* w low   */
    hdr[5]  = (uint8_t)(WALLPAPER_W >> 8);          /* w high  */
    hdr[6]  = (uint8_t)(WALLPAPER_H & 0xFF);       /* h low   */
    hdr[7]  = (uint8_t)(WALLPAPER_H >> 8);          /* h high  */
    hdr[8]  = (uint8_t)(WALLPAPER_STRIDE & 0xFF);   /* stride low  */
    hdr[9]  = (uint8_t)(WALLPAPER_STRIDE >> 8);      /* stride high */
    hdr[10] = 0x00;  /* reserved_2 (low)  */
    hdr[11] = 0x00;  /* reserved_2 (high) */

    return w25q64_write(WALLPAPER_ADDR, hdr, HEADER_SIZE);
}

/**
 * @brief  校验 W25Q64 是否已有壁纸
 */
bool w25q64_program_is_done(void)
{
    if (w25q64_get_capacity() == 0) return false;

    uint8_t magic;
    if (!w25q64_read(WALLPAPER_ADDR, &magic, 1)) return false;

    return (magic == 0x19);
}

/**
 * @brief  烧录壁纸到 W25Q64
 * @note   擦除：2 个 64KB 块 + 1 个 4KB 子扇区（覆盖 ~132KB）
 */
bool w25q64_program_wallpaper(void)
{
    /* 先检是否已烧录 */
    if (w25q64_program_is_done()) return true;

    /* 擦除目标区域 */
    if (!w25q64_erase_block(WALLPAPER_ADDR))             return false;
    if (!w25q64_erase_block(WALLPAPER_ADDR + 0x10000))   return false;
    if (!w25q64_erase_subsector(WALLPAPER_ADDR + 0x20000)) return false;

    /* 写 header */
    if (!write_header()) return false;

    /* 写像素数据 */
    if (!w25q64_write(WALLPAPER_ADDR + HEADER_SIZE,
                      wallpaper_map,
                      WALLPAPER_H * WALLPAPER_STRIDE)) {
        return false;
    }

    /* 校验 header */
    return w25q64_program_is_done();
}
