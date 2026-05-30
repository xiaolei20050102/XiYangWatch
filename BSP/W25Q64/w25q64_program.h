/**
 * @file    w25q64_program.h
 * @brief   W25Q64 烧录工具 —— 一次性将资源数据写入外部 Flash
 */

#ifndef W25Q64_PROGRAM_H
#define W25Q64_PROGRAM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  检查 W25Q64 是否已烧录壁纸（读 header magic 校验）
 * @retval true 已有壁纸 / false 未烧录或芯片未就绪
 */
bool w25q64_program_is_done(void);

/**
 * @brief  将壁纸数据烧录到 W25Q64（一次性操作，由生成脚本提供数据）
 * @retval true 烧录成功 / false 失败
 */
bool w25q64_program_wallpaper(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_PROGRAM_H */
