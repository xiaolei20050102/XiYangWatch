/*******************************************************************************
 * Size: 24 px
 * Bpp: 1
 * Opts: --font C:/Windows/Fonts/CascadiaMono.ttf --format lvgl --bpp 1 --size 24 --range 0x2C,0x30-0x39 -o e:/XiYangWatch/XiYang_Watch/Middlewares/LVGL/fonts/cascadia_mono_24_1bpp.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef CASCADIA_MONO_24_1BPP
#define CASCADIA_MONO_24_1BPP 1
#endif

#if CASCADIA_MONO_24_1BPP

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+002C "," */
    0x6f, 0xff, 0xf6,

    /* U+0030 "0" */
    0x1f, 0x83, 0xfc, 0x79, 0xe7, 0xe, 0xe0, 0x7e,
    0x7, 0xe0, 0x7e, 0xe7, 0xee, 0x7e, 0xe7, 0xe0,
    0x7e, 0x7, 0xe0, 0x77, 0xe, 0x79, 0xe3, 0xfc,
    0x1f, 0x80,

    /* U+0031 "1" */
    0x1e, 0x1f, 0xc3, 0xf8, 0x47, 0x0, 0xe0, 0x1c,
    0x3, 0x80, 0x70, 0xe, 0x1, 0xc0, 0x38, 0x7,
    0x0, 0xe0, 0x1c, 0x3, 0x87, 0xff, 0xff, 0xe0,

    /* U+0032 "2" */
    0x1f, 0xf, 0xfb, 0x87, 0xa0, 0x70, 0xe, 0x1,
    0xc0, 0x38, 0xf, 0x1, 0xc0, 0x78, 0xe, 0x3,
    0x81, 0xe0, 0x78, 0x3c, 0x7, 0xff, 0xff, 0xe0,

    /* U+0033 "3" */
    0x1f, 0x8f, 0xfb, 0x87, 0xa0, 0x70, 0xe, 0x1,
    0xc0, 0x70, 0x78, 0x1f, 0x80, 0xf8, 0x7, 0x80,
    0x70, 0xe, 0x1, 0xc0, 0x77, 0xfe, 0xff, 0x0,

    /* U+0034 "4" */
    0x0, 0xe3, 0xe, 0x30, 0xe3, 0xe, 0x70, 0xe7,
    0xe, 0x60, 0xe6, 0xe, 0x60, 0xee, 0xe, 0xe0,
    0xef, 0xff, 0xff, 0xf0, 0xe, 0x0, 0xe0, 0xe,
    0x0, 0xe0,

    /* U+0035 "5" */
    0x7f, 0xe7, 0xfe, 0x70, 0x7, 0x0, 0x70, 0x7,
    0x0, 0x7f, 0x87, 0xfe, 0x60, 0xe0, 0x7, 0x0,
    0x70, 0x7, 0xe0, 0x7e, 0x7, 0x70, 0xe7, 0xfc,
    0x1f, 0x80,

    /* U+0036 "6" */
    0x3, 0xc0, 0xfc, 0x1e, 0x3, 0x80, 0x70, 0x7,
    0x0, 0xe7, 0xce, 0xfe, 0xf0, 0xee, 0x7, 0xe0,
    0x7e, 0x7, 0xe0, 0x7e, 0x7, 0x70, 0xe7, 0xfc,
    0x1f, 0x80,

    /* U+0037 "7" */
    0xff, 0xff, 0xff, 0xb8, 0x1d, 0xc0, 0xee, 0x6,
    0x70, 0x70, 0x3, 0x80, 0x18, 0x1, 0xc0, 0xe,
    0x0, 0x60, 0x7, 0x0, 0x38, 0x1, 0x80, 0x1c,
    0x0, 0xe0, 0x6, 0x0,

    /* U+0038 "8" */
    0x1f, 0x87, 0xfc, 0xf1, 0xee, 0xe, 0xe0, 0xef,
    0x1e, 0x7f, 0xc3, 0xf8, 0x7f, 0xe7, 0xe, 0xe0,
    0x7e, 0x7, 0xe0, 0x7e, 0x7, 0x70, 0xe7, 0xfe,
    0x1f, 0x80,

    /* U+0039 "9" */
    0x1f, 0x83, 0xfe, 0x70, 0xee, 0x7, 0xe0, 0x7e,
    0x7, 0xe0, 0x7e, 0x7, 0xf0, 0xf7, 0xf7, 0x3e,
    0x70, 0xe, 0x0, 0xe0, 0x1c, 0x7, 0x83, 0xf0,
    0x3c, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 225, .box_w = 3, .box_h = 8, .ofs_x = 5, .ofs_y = -5},
    {.bitmap_index = 3, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 29, .adv_w = 225, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 53, .adv_w = 225, .box_w = 11, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 225, .box_w = 11, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 101, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 225, .box_w = 13, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 225, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 0, 0, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 44, .range_length = 14, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 14, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t cascadia_mono_24_1bpp = {
#else
lv_font_t cascadia_mono_24_1bpp = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 22,          /*The maximum line height required by the font*/
    .base_line = 5,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if CASCADIA_MONO_24_1BPP*/

