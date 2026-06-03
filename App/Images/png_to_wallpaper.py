"""将任意 PNG/JPG 图片转为 240×280 RGB565 LVGL 壁纸 C 文件
用法: python png_to_wallpaper.py <图片路径>
输出: wallpaper.c (放到 App/Images/Wallpaper/)
"""

import sys
import os

try:
    from PIL import Image
except ImportError:
    print("需要 Pillow: pip install Pillow")
    sys.exit(1)

SRC = sys.argv[1]
W, H = 240, 280
OUT_DIR = os.path.join(os.path.dirname(__file__), "Wallpapers")
OUT_C = os.path.join(OUT_DIR, "wallpaper.c")
OUT_H = os.path.join(OUT_DIR, "wallpaper.h")

os.makedirs(OUT_DIR, exist_ok=True)

# 1. 打开图片并缩放到 240×280
img = Image.open(SRC).convert("RGB")
img = img.resize((W, H), Image.LANCZOS)

# 2. 像素 → RGB565 (low byte first = LV_COLOR_FORMAT_RGB565)
pixels = list(img.getdata())
data = bytearray()
for r, g, b in pixels:
    rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    data.append(rgb565 & 0xFF)       # 低字节
    data.append((rgb565 >> 8) & 0xFF)  # 高字节

DS = len(data)
print(f"图片 {img.size} → RGB565 {DS} bytes ({DS/1024:.1f} KB)")

# 3. 写 .c 文件
c_lines = [
    '#include "wallpaper.h"',
    '',
    'const unsigned char wallpaper_map[] = {',
]
for i in range(0, DS, 16):
    chunk = data[i:i+16]
    c_lines.append('    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',')

c_lines += [
    '};',
    '',
    f'const lv_image_dsc_t wallpaper_img = {{',
    f'    .header.cf = LV_COLOR_FORMAT_RGB565,',
    f'    .header.magic = LV_IMAGE_HEADER_MAGIC,',
    f'    .header.w = {W},',
    f'    .header.h = {H},',
    f'    .data_size = {DS},',
    f'    .data = wallpaper_map,',
    f'}};',
]

with open(OUT_C, 'w', encoding='utf-8') as f:
    f.write('\n'.join(c_lines) + '\n')

# 4. 写 .h 文件
with open(OUT_H, 'w', encoding='utf-8') as f:
    f.write(f'''#ifndef WALLPAPER_H
#define WALLPAPER_H

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

extern const lv_image_dsc_t wallpaper_img;

#endif
''')

print(f"生成: {OUT_C}")
print(f"生成: {OUT_H}")
print("完成，添加到编译即可")
