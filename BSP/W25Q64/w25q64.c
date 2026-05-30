/**
 * @file    w25q64.c
 * @brief   W25Q64 外部 SPI Flash 驱动实现
 * @note    通过函数指针操作硬件，不依赖具体 MCU / HAL
 */

#include "w25q64.h"

/* ═══════════════════════════════════════════════════════════════
 * 硬件抽象指针（由 w25q64_init 注入）
 * ═══════════════════════════════════════════════════════════════ */

static w25q_spi_xfer_t  spi_xfer;
static w25q_cs_ctrl_t   cs_ctrl;
static w25q_delay_ms_t  delay_ms;
static w25q_tick_t      tick;

static uint16_t         jedec_id;
static uint32_t         capacity;

/* ═══════════════════════════════════════════════════════════════
 * 内部辅助
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  CS 拉低，使能芯片
 */
static inline void cs_low(void)  { cs_ctrl(true); }

/**
 * @brief  CS 拉高，禁用芯片
 */
static inline void cs_high(void) { cs_ctrl(false); }

/**
 * @brief  发送命令 + 可选数据，同时接收响应
 * @param  cmd      命令字节数组
 * @param  cmd_len  命令长度
 * @param  rx       接收缓冲区（可为 NULL）
 * @param  rx_len   接收长度（可为 0）
 */
static void spi_cmd(const uint8_t *cmd, uint32_t cmd_len,
                    uint8_t *rx, uint32_t rx_len)
{
    cs_low();
    if (cmd && cmd_len > 0) {
        spi_xfer(cmd, NULL, cmd_len);
    }
    if (rx && rx_len > 0) {
        spi_xfer(NULL, rx, rx_len);
    }
    cs_high();
}

/**
 * @brief  读取状态寄存器 1
 * @retval 8 位状态值
 */
static uint8_t read_status1(void)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    uint8_t sr  = 0;
    spi_cmd(&cmd, 1, &sr, 1);
    return sr;
}

/**
 * @brief  等待芯片空闲（BUSY 位清零）
 * @param  timeout_ms  超时毫秒数
 * @retval true 空闲 / false 超时
 */
static bool wait_ready(uint32_t timeout_ms)
{
    uint32_t start = tick();
    do {
        if ((read_status1() & W25Q_SR1_BUSY) == 0) {
            return true;
        }
    } while ((tick() - start) < timeout_ms);
    return false;
}

/**
 * @brief  发送写使能，并等待 WEL 位置位
 * @retval true 成功 / false 超时
 */
static bool write_enable(void)
{
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    spi_cmd(&cmd, 1, NULL, 0);

    /* 等待 WEL 置位 */
    uint32_t start = tick();
    do {
        if (read_status1() & W25Q_SR1_WEL) {
            return true;
        }
    } while ((tick() - start) < 100U);
    return false;
}

/* ═══════════════════════════════════════════════════════════════
 * 公共 API
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  初始化 W25Q64 并自动检测芯片 ID
 */
bool w25q64_init(w25q_spi_xfer_t xfer_fn, w25q_cs_ctrl_t cs_fn,
                 w25q_delay_ms_t dly_fn, w25q_tick_t tck_fn)
{
    if (!xfer_fn || !cs_fn || !dly_fn || !tck_fn) return false;

    spi_xfer = xfer_fn;
    cs_ctrl  = cs_fn;
    delay_ms = dly_fn;
    tick     = tck_fn;

    /* 硬件复位 */
    {
        uint8_t cmd[2] = { W25Q_CMD_RESET_ENABLE, W25Q_CMD_RESET_DEVICE };
        cs_low();
        spi_xfer(cmd, NULL, 2);
        cs_high();
    }
    delay_ms(10);

    /* 读取 JEDEC ID */
    {
        uint8_t cmd = W25Q_CMD_READ_JEDEC_ID;
        uint8_t id[3] = { 0 };
        spi_cmd(&cmd, 1, id, 3);
        jedec_id = ((uint16_t)id[0] << 8) | id[2];
    }

    /* 根据 ID 确定容量 */
    if (jedec_id >= 0xEF17)      capacity = 16U * 1024U * 1024U;  /* W25Q128 */
    else if (jedec_id >= 0xEF16) capacity =  8U * 1024U * 1024U;  /* W25Q64  */
    else if (jedec_id >= 0xEF15) capacity =  4U * 1024U * 1024U;  /* W25Q32  */
    else if (jedec_id >= 0xEF14) capacity =  2U * 1024U * 1024U;  /* W25Q16  */
    else if (jedec_id >= 0xEF13) capacity =  1U * 1024U * 1024U;  /* W25Q80  */
    else                         capacity =  0;

    return capacity > 0;
}

/**
 * @brief  读取数据
 */
bool w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!buf || len == 0) return false;

    uint8_t cmd[4] = {
        W25Q_CMD_READ_DATA,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr)
    };

    cs_low();
    spi_xfer(cmd, NULL, 4);
    spi_xfer(NULL, buf, len);
    cs_high();

    return true;
}

/**
 * @brief  写入数据（自动跨页）
 */
bool w25q64_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (!buf || len == 0) return false;

    uint32_t end = addr + len;

    while (addr < end) {
        /* 计算当前页剩余空间 */
        uint32_t page_remain = W25Q64_PAGE_SIZE - (addr & (W25Q64_PAGE_SIZE - 1));
        uint32_t chunk = (end - addr < page_remain) ? (end - addr) : page_remain;

        if (!write_enable()) return false;

        uint8_t cmd[4] = {
            W25Q_CMD_PAGE_PROGRAM,
            (uint8_t)(addr >> 16),
            (uint8_t)(addr >> 8),
            (uint8_t)(addr)
        };

        cs_low();
        spi_xfer(cmd, NULL, 4);
        spi_xfer(buf, NULL, chunk);
        cs_high();

        if (!wait_ready(W25Q_TIMEOUT_PAGE_WRITE)) return false;

        addr += chunk;
        buf  += chunk;
    }

    return true;
}

/**
 * @brief  擦除 4KB 子扇区
 */
bool w25q64_erase_subsector(uint32_t addr)
{
    if (!write_enable()) return false;

    uint8_t cmd[4] = {
        W25Q_CMD_SECTOR_ERASE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr)
    };
    spi_cmd(cmd, 4, NULL, 0);

    return wait_ready(W25Q_TIMEOUT_SECTOR_ERASE);
}

/**
 * @brief  擦除 64KB 块
 */
bool w25q64_erase_block(uint32_t addr)
{
    if (!write_enable()) return false;

    uint8_t cmd[4] = {
        W25Q_CMD_BLOCK_ERASE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr)
    };
    spi_cmd(cmd, 4, NULL, 0);

    return wait_ready(W25Q_TIMEOUT_BLOCK_ERASE);
}

/**
 * @brief  全片擦除
 */
bool w25q64_erase_chip(void)
{
    if (!write_enable()) return false;

    uint8_t cmd = W25Q_CMD_CHIP_ERASE;
    spi_cmd(&cmd, 1, NULL, 0);

    return wait_ready(W25Q_TIMEOUT_CHIP_ERASE);
}

/**
 * @brief  获取 JEDEC ID
 */
uint16_t w25q64_get_jedec_id(void)
{
    return jedec_id;
}

/**
 * @brief  获取容量
 */
uint32_t w25q64_get_capacity(void)
{
    return capacity;
}

/**
 * @brief  进入低功耗
 */
void w25q64_power_down(void)
{
    uint8_t cmd = W25Q_CMD_POWER_DOWN;
    spi_cmd(&cmd, 1, NULL, 0);
}

/**
 * @brief  退出低功耗
 */
void w25q64_wakeup(void)
{
    uint8_t cmd = W25Q_CMD_RELEASE_POWERDN;
    spi_cmd(&cmd, 1, NULL, 0);
    delay_ms(3);
}
