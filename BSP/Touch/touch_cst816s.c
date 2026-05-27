/**
 * @file    touch_cst816s.c
 * @author  xiyangdaxia
 * @brief   CST816S 电容触摸驱动实现（I2C1，地址 0x15，硬件抽象）
 * @date    2026-05-21
 * @version v1.4
 *
 * @details 本文件不直接调用 HAL，所有硬件操作通过 touch_cst816s.h 中的宏完成
 *          I2C 总线与 QMI8658 / MAX30102 / BME280 / BH1750 / DS3231 共享
 *          参考 OV-Watch：手势判断交给上层，驱动只负责手指数 + 坐标
 *
 * @note    更换模组需确认 TOUCH_OFFSET_Y 值
 *
 * @par 修改记录:
 * - 2026-05-21  v1.0  创建文件，实现初始化、坐标读取、手势识别、休眠唤醒
 * - 2026-05-21  v1.1  新增 CST816_Test 自检函数
 * - 2026-05-21  v1.2  尝试软件双击状态机（已废弃）
 * - 2026-05-21  v1.3  回归硬件手势寄存器方案（已废弃）
 * - 2026-05-21  v1.4  改为 OV-Watch 方案：只读手指数+坐标，手势判断交给上层
 */

#include "touch_cst816s.h"
#include "usart.h"
#include "string.h"

/* ========================== 全局函数实现 ========================== */

/**
 * @brief   CST816S 初始化：复位 + 配置自动休眠时间
 * @param   无
 * @retval  无
 * @note    复位时序：RST 低 10ms → 高 100ms
 *          自动休眠设为 5 秒无触摸后进入低功耗
 */
void CST816_Init(void)
{
    /* 硬件复位 */
    TOUCH_RST_LOW();
    TOUCH_DELAY_MS(10);
    TOUCH_RST_HIGH();
    TOUCH_DELAY_MS(100);

    /* 5 秒无触摸自动进入低功耗 */
    uint8_t t = 5;
    TOUCH_I2C_WRITE(REG_AutoSleepTime, &t, 1);
}

/**
 * @brief   读取芯片 ID
 * @param   无
 * @retval  uint8_t  芯片 ID 值
 * @note    CST816S 芯片 ID 通常在 0xB4~0xB6 范围，用于验证通信是否正常
 */
uint8_t CST816_GetChipID(void)
{
    uint8_t id = 0;
    TOUCH_I2C_READ(REG_ChipID, &id, 1);
    return id;
}

/**
 * @brief   读取当前触摸手指数量
 * @param   无
 * @retval  uint8_t  手指数量（0 = 无触摸，0xFF = 睡眠状态）
 * @note    返回 0xFF 表示触摸芯片已进入休眠，需调用 CST816_Wakeup
 */
uint8_t CST816_GetFingerNum(void)
{
    uint8_t num = 0;
    TOUCH_I2C_READ(REG_FingerNum, &num, 1);
    return num;
}

/**
 * @brief   读取触摸坐标
 * @param   info  [出] 坐标结构体指针，X/Y 坐标写入其中
 * @retval  无
 * @note    从寄存器 0x03~0x06 连续读取 4 字节，自动叠加 TOUCH_OFFSET_Y
 *          X = (buf[0]&0x0F)<<8 | buf[1]
 *          Y = (buf[2]&0x0F)<<8 | buf[3] + TOUCH_OFFSET_Y
 */
void CST816_GetTouch(Touch_Info_t* info)
{
    uint8_t buf[4] = {0};
    TOUCH_I2C_READ(REG_XposH, buf, 4);

    info->X_Pos = ((buf[0] & 0x0F) << 8) | buf[1];
    info->Y_Pos = ((buf[2] & 0x0F) << 8) | buf[3] + TOUCH_OFFSET_Y;
}

/**
 * @brief   读取手势类型
 * @param   无
 * @retval  Touch_Gesture_t  手势枚举值（NONE/DOWN/UP/LEFT/RIGHT/CLICK/DOUBLECLICK/LONGPRESS）
 * @note    读完后手势寄存器自动清零，下次读取前新手势才会出现
 */
Touch_Gesture_t CST816_GetGesture(void)
{
    uint8_t g = 0;
    TOUCH_I2C_READ(REG_GestureID, &g, 1);
    /* 手册说明读后自动清零，无需额外写 0，多余的 I2C 写可能干扰触摸状态 */
    return (Touch_Gesture_t)g;
}

/**
 * @brief   进入睡眠模式
 * @param   无
 * @retval  无
 * @note    睡眠后无法触摸唤醒，需调用 CST816_Wakeup 恢复
 */
void CST816_Sleep(void)
{
    uint8_t cmd = 0x03;
    TOUCH_I2C_WRITE(REG_SleepMode, &cmd, 1);
}

/**
 * @brief   唤醒触摸芯片
 * @param   无
 * @retval  无
 * @note    通过硬件复位引脚重新复位芯片来唤醒
 */
void CST816_Wakeup(void)
{
    TOUCH_RST_LOW();
    TOUCH_DELAY_MS(10);
    TOUCH_RST_HIGH();
    TOUCH_DELAY_MS(100);
}

/**
 * @brief  CST816S 全面自检：芯片ID → 休眠检测 → 触摸轮询 → 手势识别 → 休眠唤醒
 * @param   无
 * @retval  无
 * @note    通过 UART2 输出测试过程和结果，方便调试
 *          轮询阶段持续约 5 秒，期间触摸屏幕可观察坐标和手势上报
 *          测试结束输出 "PASS" 或 "FAIL" 汇总
 */
void CST816_Test(void)
{
    uint8_t buf[64];
    uint8_t fail = 0;

    /* === 头 === */
    {
        uint8_t msg[] = "\r\n=== CST816S Touch Test ===\r\n";
        HAL_UART_Transmit(&huart2, msg, strlen((char*)msg), 1000);
    }

    /* === 1. 芯片 ID === */
    {
        uint8_t id = CST816_GetChipID();
        sprintf((char*)buf,
                "[CHIP ID] 0x%02X (%s)\r\n",
                id, (id >= 0xB4 && id <= 0xB6) ? "OK" : "FAIL");
        HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
        if (id < 0xB4 || id > 0xB6) fail++;
    }

    /* === 2. 休眠状态检测 === */
    {
        uint8_t num = CST816_GetFingerNum();
        if (num == 0xFF) {
            uint8_t msg[] = "[SLEEP] 芯片处于休眠状态，尝试唤醒...\r\n";
            HAL_UART_Transmit(&huart2, msg, strlen((char*)msg), 1000);
            CST816_Wakeup();
            TOUCH_DELAY_MS(120);
            num = CST816_GetFingerNum();
            sprintf((char*)buf,
                    "[WAKE ] FingerNum=0x%02X (%s)\r\n",
                    num, (num != 0xFF) ? "OK" : "FAIL");
            HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
            if (num == 0xFF) fail++;
        } else {
            sprintf((char*)buf,
                    "[STATE] FingerNum=0x%02X (OK)\r\n", num);
            HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
        }
    }

    /* === 3. 触摸轮询 10 秒（手指数 + 坐标，参考 OV-Watch） === */
    {
        uint8_t msg[] = "[POLL ] 10秒触摸测试 — 按下/抬起 + 坐标\r\n";
        HAL_UART_Transmit(&huart2, msg, strlen((char*)msg), 1000);

        uint32_t end = HAL_GetTick() + 10000;
        uint8_t was_pressed = 0;
        while (HAL_GetTick() < end) {
            uint8_t finger = CST816_GetFingerNum();
            uint8_t pressed = (finger != 0x00 && finger != 0xFF);

            if (pressed && !was_pressed) {
                Touch_Info_t info;
                CST816_GetTouch(&info);
                sprintf((char*)buf,
                        "[TOUCH] PRESS   X=%03u Y=%03u\r\n",
                        info.X_Pos, info.Y_Pos);
                HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
            } else if (!pressed && was_pressed) {
                Touch_Info_t info;
                CST816_GetTouch(&info);
                sprintf((char*)buf,
                        "[TOUCH] RELEASE X=%03u Y=%03u\r\n",
                        info.X_Pos, info.Y_Pos);
                HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
            }
            was_pressed = pressed;
            TOUCH_DELAY_MS(10);
        }
    }

    /* === 4. 休眠/唤醒循环测试 === */
    {
        uint8_t msg[] = "[CYCLE] 休眠→唤醒 循环测试...\r\n";
        HAL_UART_Transmit(&huart2, msg, strlen((char*)msg), 1000);

        CST816_Sleep();
        TOUCH_DELAY_MS(50);
        uint8_t num = CST816_GetFingerNum();
        sprintf((char*)buf,
                "[CYCLE] 休眠后 FingerNum=0x%02X (OK)\r\n", num);
        HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);

        CST816_Wakeup();
        TOUCH_DELAY_MS(120);
        num = CST816_GetFingerNum();
        sprintf((char*)buf,
                "[CYCLE] 唤醒后 FingerNum=0x%02X (%s)\r\n",
                num, (num != 0xFF) ? "OK" : "FAIL");
        HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
        if (num == 0xFF) fail++;
    }

    /* === 结果汇总 === */
    {
        sprintf((char*)buf,
                "\r\n=== CST816S Test: %s (%u error(s)) ===\r\n\r\n",
                (fail == 0) ? "PASS" : "FAIL", fail);
        HAL_UART_Transmit(&huart2, buf, strlen((char*)buf), 1000);
    }
}
