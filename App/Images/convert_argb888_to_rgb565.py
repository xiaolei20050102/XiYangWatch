"""Convert LVGL ARGB8888 image .c files to RGB565, optionally baking in a recolor.

Usage: python convert_argb888_to_rgb565.py <file.c>:<RRGGBB> ...

ARGB8888 bytes are stored as B,G,R,A (little-endian uint32).
Output RGB565 (2 bytes/pixel, big-endian uint16).
If recolor is provided, the original RGB is replaced by it before alpha blending.
"""

import re
import sys
import os

def parse_c_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    w = int(re.search(r'\.header\.w\s*=\s*(\d+)', content).group(1))
    h = int(re.search(r'\.header\.h\s*=\s*(\d+)', content).group(1))
    name = re.search(r'const lv_image_dsc_t\s+(\w+)\s*=', content).group(1)
    map_name = re.search(r'(\w+_map)\[', content).group(1)
    hex_data = bytes(int(h, 16) for h in re.findall(r'0x[0-9a-fA-F]{2}', content))
    return name, map_name, w, h, hex_data

def convert(data, w, h, recolor=None):
    """BGRA little-endian -> RGB565 big-endian, optionally with recolor."""
    out = bytearray()
    if recolor:
        rr, rg, rb = recolor
    for i in range(0, len(data), 4):
        b, g, r, a = data[i], data[i+1], data[i+2], data[i+3]
        if recolor:
            r, g, b = rr, rg, rb
        r2 = (r * a) // 255
        g2 = (g * a) // 255
        b2 = (b * a) // 255
        rgb565 = ((r2 >> 3) << 11) | ((g2 >> 2) << 5) | (b2 >> 3)
        out.append(rgb565 & 0xFF)
        out.append((rgb565 >> 8) & 0xFF)
    return bytes(out)

def write_rgb565_c(path, name, map_name, w, h, data):
    out_path = path.replace('.c', '_rgb565.c')
    guard = name.upper()
    map_new = map_name.replace('_map', '_rgb565_map')
    name_new = f"{name}_rgb565"

    lines = [
        '#ifdef __has_include',
        '    #if __has_include("lvgl.h")',
        '        #ifndef LV_LVGL_H_INCLUDE_SIMPLE',
        '            #define LV_LVGL_H_INCLUDE_SIMPLE',
        '        #endif',
        '    #endif',
        '#endif',
        '',
        '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)',
        '    #include "lvgl.h"',
        '#else',
        '    #include "lvgl/lvgl.h"',
        '#endif',
        '',
        '#ifndef LV_ATTRIBUTE_MEM_ALIGN',
        '#define LV_ATTRIBUTE_MEM_ALIGN',
        '#endif',
        '',
        f'#ifndef LV_ATTRIBUTE_IMAGE_{guard}',
        f'#define LV_ATTRIBUTE_IMAGE_{guard}',
        '#endif',
        '',
        f'const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{guard} uint8_t {map_new}[] = {{',
    ]

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        lines.append('  ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',')

    ds = w * h * 2
    lines += [
        '};',
        '',
        f'const lv_image_dsc_t {name_new} = {{',
        f'  .header.cf = LV_COLOR_FORMAT_RGB565,',
        f'  .header.magic = LV_IMAGE_HEADER_MAGIC,',
        f'  .header.w = {w},',
        f'  .header.h = {h},',
        f'  .data_size = {ds},',
        f'  .data = {map_new},',
        '};',
    ]

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f"  -> {out_path}  ({w}x{h}, {ds} bytes)")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python convert_argb888_to_rgb565.py <file.c>:<RRGGBB> ...")
        print("  RRGGBB = recolor hex, or 'none' to keep original colors")
        sys.exit(1)

    for arg in sys.argv[1:]:
        parts = arg.split(':')
        path = parts[0]
        recolor = None
        if len(parts) > 1 and parts[1].lower() != 'none':
            hx = parts[1]
            recolor = (int(hx[0:2], 16), int(hx[2:4], 16), int(hx[4:6], 16))
        print(f"Converting: {path}  {'recolor=#'+parts[1] if recolor else 'keep original'}")
        name, map_name, w, h, hex_data = parse_c_file(path)
        rgb565_data = convert(hex_data, w, h, recolor)
        write_rgb565_c(path, name, map_name, w, h, rgb565_data)
    print("Done.")
