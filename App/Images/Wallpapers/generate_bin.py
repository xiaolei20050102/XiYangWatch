"""将 PNG 壁纸转为 LVGL .bin 文件 + C 数组编程文件
用法: python generate_bin.py <图片路径>
输出: wallpaper.bin (可直接写入 W25Q64) + wallpaper_program.c (一次性烧录用)
"""

import sys
import os
import struct

try:
    from PIL import Image
except ImportError:
    print("需要 Pillow: pip install Pillow")
    sys.exit(1)

SRC = sys.argv[1]
W, H = 240, 280

OUT_DIR = os.path.dirname(__file__)
OUT_BIN = os.path.join(OUT_DIR, "wallpaper.bin")
OUT_C   = os.path.join(OUT_DIR, "wallpaper_program.c")

# LVGL 常量
LV_IMAGE_HEADER_MAGIC  = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12

# 1. 打开图片
img = Image.open(SRC).convert("RGB")
img = img.resize((W, H), Image.LANCZOS)

# 2. 像素 → RGB565 (low byte first, little-endian)
pixels = list(img.getdata())
pixel_data = bytearray()
for r, g, b in pixels:
    rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    pixel_data.append(rgb565 & 0xFF)
    pixel_data.append((rgb565 >> 8) & 0xFF)

stride = W * 2  # 240 * 2 bytes/pixel

# 3. LVGL image header (little-endian bitfields)
header = struct.pack('<BBHHHHH',
    LV_IMAGE_HEADER_MAGIC,   # magic     (1B)
    LV_COLOR_FORMAT_RGB565,  # cf        (1B)
    0,                       # flags     (2B)
    W,                       # w         (2B)
    H,                       # h         (2B)
    stride,                  # stride    (2B)
    0,                       # reserved_2(2B)
)
print(f"Header size: {len(header)} bytes")
print(f"Packet: magic=0x{LV_IMAGE_HEADER_MAGIC:02X}, cf=0x{LV_COLOR_FORMAT_RGB565:02X}, w={W}, h={H}, stride={stride}")

bin_data = header + pixel_data
total_size = len(bin_data)
print(f"Image {img.size} → RGB565 {len(pixel_data)} bytes")
print(f"Total .bin: {total_size} bytes ({total_size/1024:.1f} KB)")

# 4. Write .bin
with open(OUT_BIN, 'wb') as f:
    f.write(bin_data)
print(f"生成: {OUT_BIN}")

# 5. Write .c programmer file (与 C 源码内的 header 保持独立)
c_lines = []
c_lines.append('// 自动生成，一次性用于烧录 W25Q64')
c_lines.append('// 烧录完成后可从构建中移除本文件')
c_lines.append('#include "w25q64.h"')
c_lines.append('#include <stdint.h>')
c_lines.append('')
c_lines.append(f'#define PROG_TOTAL_SIZE {total_size}U')
c_lines.append('')
c_lines.append('static const uint8_t wallpaper_prog_data[] = {')

for i in range(0, len(bin_data), 16):
    chunk = bin_data[i:i+16]
    c_lines.append('    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',')

c_lines.append('};')
c_lines.append('')

c_lines.append('''/**
 * @brief  将壁纸数据写入 W25Q64（一次性操作）
 * @param  start_addr  写入起始地址（建议 0x000000）
 * @retval true 写入成功 / false 失败
 */
bool w25q64_program_wallpaper(uint32_t start_addr)
{
    /* 先校验数据区域的 header magic */
    if (wallpaper_prog_data[0] != 0x19) return false;

    /* 擦除所需范围：壁纸占 ~132KB，擦除 2 个 64KB 块 + 1 个 4KB 子扇区 */
    if (!w25q64_erase_block(start_addr))            return false;
    if (!w25q64_erase_block(start_addr + 0x10000))  return false;
    if (!w25q64_erase_subsector(start_addr + 0x20000)) return false;

    /* 写入 */
    if (!w25q64_write(start_addr, wallpaper_prog_data, PROG_TOTAL_SIZE)) return false;

    /* 校验：读回 header 检查 */
    uint8_t verify[4];
    if (!w25q64_read(start_addr, verify, 4)) return false;
    if (verify[0] != 0x19 || verify[1] != 0x12) return false;

    return true;
}''')

with open(OUT_C, 'w', encoding='utf-8') as f:
    f.write('\n'.join(c_lines) + '\n')
print(f"生成: {OUT_C}")
print("完成。")
