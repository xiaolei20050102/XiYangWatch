/**
 * @file    w25q64_fs.h
 * @brief   W25Q64 LVGL 文件系统驱动 —— 将 W25Q64 映射为 LVGL 虚拟文件
 * @note    路径格式 "W:/filename" → 内部查表转为 Flash 地址
 */

#ifndef W25Q64_FS_H
#define W25Q64_FS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  注册 LVGL 文件系统驱动（驱动盘符 'W'）
 * @note   调用一次即可，需在 w25q64_port_init() 之后调用
 */
void w25q64_fs_init(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_FS_H */
