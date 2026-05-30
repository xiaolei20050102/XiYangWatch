/**
 * @file    w25q64.h
 * @brief   W25Q64 外部 SPI Flash 驱动 —— 硬件无关接口
 * @note    通过函数指针注入 SPI / CS / 延时 / 滴答，可移植到任意 MCU
 */

#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 芯片参数
 * ═══════════════════════════════════════════════════════════════ */

#define W25Q64_FLASH_SIZE         0x800000U  /**< 8 MBytes  */
#define W25Q64_SECTOR_SIZE        0x10000U   /**< 64 KB      */
#define W25Q64_SECTOR_COUNT       128U       /**< 128 扇区   */
#define W25Q64_SUBSECTOR_SIZE     0x1000U    /**< 4 KB       */
#define W25Q64_SUBSECTOR_COUNT    2048U      /**< 2048 子扇区 */
#define W25Q64_PAGE_SIZE          0x100U     /**< 256 bytes  */

/* ═══════════════════════════════════════════════════════════════
 * 命令表
 * ═══════════════════════════════════════════════════════════════ */

#define W25Q_CMD_WRITE_ENABLE     0x06U
#define W25Q_CMD_WRITE_DISABLE    0x04U
#define W25Q_CMD_READ_STATUS1     0x05U
#define W25Q_CMD_READ_STATUS2     0x35U
#define W25Q_CMD_WRITE_STATUS     0x01U
#define W25Q_CMD_PAGE_PROGRAM     0x02U
#define W25Q_CMD_SECTOR_ERASE     0x20U  /**< 4KB 子扇区擦除 */
#define W25Q_CMD_BLOCK_ERASE      0xD8U  /**< 64KB 块擦除    */
#define W25Q_CMD_CHIP_ERASE       0xC7U  /**< 全片擦除       */
#define W25Q_CMD_READ_DATA        0x03U
#define W25Q_CMD_FAST_READ        0x0BU
#define W25Q_CMD_READ_JEDEC_ID    0x9FU
#define W25Q_CMD_READ_UNIQUE_ID   0x4BU
#define W25Q_CMD_POWER_DOWN       0xB9U
#define W25Q_CMD_RELEASE_POWERDN  0xABU
#define W25Q_CMD_RESET_ENABLE     0x66U
#define W25Q_CMD_RESET_DEVICE     0x99U

/* 状态寄存器位 */
#define W25Q_SR1_BUSY             0x01U
#define W25Q_SR1_WEL              0x02U

/* 超时 (ms) */
#define W25Q_TIMEOUT_PAGE_WRITE   3U
#define W25Q_TIMEOUT_SECTOR_ERASE 400U
#define W25Q_TIMEOUT_BLOCK_ERASE  2000U
#define W25Q_TIMEOUT_CHIP_ERASE   80000U

/* ═══════════════════════════════════════════════════════════════
 * 硬件抽象：由移植层实现，注入到 w25q64_init()
 * ═══════════════════════════════════════════════════════════════ */

/** SPI 收发回调：同时发送 tx 并接收 rx，长度为 len */
typedef void (*w25q_spi_xfer_t)(const uint8_t *tx, uint8_t *rx, uint32_t len);

/** CS 引脚控制回调 */
typedef void (*w25q_cs_ctrl_t)(bool enable);  /**< true = 片选拉低 (使能) */

/** 毫秒延时回调 */
typedef void (*w25q_delay_ms_t)(uint32_t ms);

/** 系统滴答回调，返回启动以来的毫秒数 */
typedef uint32_t (*w25q_tick_t)(void);

/* ═══════════════════════════════════════════════════════════════
 * 公共 API
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化 W25Q64 并自动检测芯片 ID
 * @param  spi_xfer   SPI 收发函数
 * @param  cs_ctrl    CS 引脚控制函数
 * @param  delay_ms   毫秒延时函数
 * @param  tick       系统滴答函数
 * @retval true 成功 / false 失败（芯片 ID 不匹配或通信异常）
 */
bool w25q64_init(w25q_spi_xfer_t spi_xfer, w25q_cs_ctrl_t cs_ctrl,
                 w25q_delay_ms_t delay_ms, w25q_tick_t tick);

/**
 * @brief  读取数据
 * @param  addr    起始地址 (0 ~ 8MB)
 * @param  buf     输出缓冲区
 * @param  len     读取长度
 * @retval true 成功 / false 超时
 */
bool w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief  写入数据（自动跨页处理）
 * @param  addr    起始地址
 * @param  buf     数据缓冲区
 * @param  len     写入长度
 * @retval true 成功 / false 超时
 */
bool w25q64_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief  擦除 4KB 子扇区
 * @param  addr    扇区内任意地址（自动对齐到 4KB 边界）
 * @retval true 成功 / false 超时
 */
bool w25q64_erase_subsector(uint32_t addr);

/**
 * @brief  擦除 64KB 块
 * @param  addr    块内任意地址（自动对齐到 64KB 边界）
 * @retval true 成功 / false 超时
 */
bool w25q64_erase_block(uint32_t addr);

/**
 * @brief  全片擦除（耗时约 60~80 秒，谨慎调用）
 * @retval true 成功 / false 超时
 */
bool w25q64_erase_chip(void);

/**
 * @brief  获取芯片 JEDEC ID (0xEF16 = W25Q64)
 * @retval 16-bit 制造商+设备 ID
 */
uint16_t w25q64_get_jedec_id(void);

/**
 * @brief  获取 Flash 总容量 (bytes)
 */
uint32_t w25q64_get_capacity(void);

/**
 * @brief  进入低功耗模式
 */
void w25q64_power_down(void);

/**
 * @brief  退出低功耗模式
 */
void w25q64_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_H */
