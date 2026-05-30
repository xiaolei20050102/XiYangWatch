/**
 * @file    w25q64_port.h
 * @brief   W25Q64 移植层 —— STM32 HAL / SPI1 / GPIO CS
 * @note    调用 w25q64_port_init() 即可完成芯片初始化
 */

#ifndef W25Q64_PORT_H
#define W25Q64_PORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 W25Q64 移植层并检测芯片
 * @retval true 成功 / false 芯片检测失败
 */
bool w25q64_port_init(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_PORT_H */
