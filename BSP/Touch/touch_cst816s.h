/**
 * @file    touch_cst816s.h
 * @author  xiyangdaxia
 * @brief   CST816S 电容触摸驱动（硬件 I2C1，地址 0x15）
 * @date    2026-05-21
 * @version v1.0
 *
 * @details 硬件抽象宏集中于此文件，移植时只需修改本文件
 *          I2C 总线与其他 5 个传感器共享（PB6/PB7），需注意互斥访问
 *
 * @note    TOUCH_OFFSET_Y 需根据实际面板确认，OV-Watch 同款模组值为 15
 *
 * @par 修改记录:
 * - 2026-05-21  v1.0  创建文件，实现基本读写、坐标读取、手势识别
 */

#ifndef __TOUCH_CST816S_H__
#define __TOUCH_CST816S_H__

/* ========================== 头文件包含 ========================== */

#include "i2c.h"

/* ========================== 引脚 & 外设定义 ========================== */

#define TOUCH_RST_PIN   GPIO_PIN_8
#define TOUCH_RST_PORT  GPIOB
#define TOUCH_INT_PIN   GPIO_PIN_2
#define TOUCH_INT_PORT  GPIOB
#define TOUCH_I2C       &hi2c1

/* ========================== 设备 & 寄存器 ========================== */

#define CST816_ADDR        0x15    /* 7-bit I2C 地址 */
#define CST816_ADDR_8BIT   ((CST816_ADDR) << 1)  /* HAL 用的 8-bit 地址 */

#define REG_GestureID       0x01
#define REG_FingerNum       0x02
#define REG_XposH           0x03
#define REG_XposL           0x04
#define REG_YposH           0x05
#define REG_YposL           0x06
#define REG_ChipID          0xA7
#define REG_SleepMode       0xE5
#define REG_MotionMask      0xEC
#define REG_IrqPluseWidth   0xED
#define REG_NorScanPer      0xEE
#define REG_MotionSlAngle   0xEF
#define REG_LpAutoWakeTime  0xF4
#define REG_LpScanTH        0xF5
#define REG_LpScanWin       0xF6
#define REG_LpScanFreq      0xF7
#define REG_LpScanIdac      0xF8
#define REG_AutoSleepTime   0xF9
#define REG_IrqCtl          0xFA
#define REG_AutoReset       0xFB
#define REG_LongPressTime   0xFC
#define REG_IOCtl           0xFD
#define REG_DisAutoSleep    0xFE

/* ========================== 面板偏移 ========================== */

#define TOUCH_OFFSET_Y  15

/* ========================== 硬件抽象宏（移植时只改这里） ========================== */

#define TOUCH_RST_LOW()   HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_RESET)
#define TOUCH_RST_HIGH()  HAL_GPIO_WritePin(TOUCH_RST_PORT, TOUCH_RST_PIN, GPIO_PIN_SET)
#define TOUCH_INT_READ()  HAL_GPIO_ReadPin(TOUCH_INT_PORT, TOUCH_INT_PIN)

#define TOUCH_I2C_WRITE(reg, pData, sz) \
    HAL_I2C_Mem_Write(TOUCH_I2C, CST816_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define TOUCH_I2C_READ(reg, pData, sz) \
    HAL_I2C_Mem_Read(TOUCH_I2C, CST816_ADDR_8BIT, (reg), I2C_MEMADD_SIZE_8BIT, (pData), (sz), 100)

#define TOUCH_DELAY_MS(ms)  HAL_Delay(ms)

/* ========================== 类型定义 ========================== */

typedef struct {
    uint16_t X_Pos;
    uint16_t Y_Pos;
} Touch_Info_t;

typedef enum {
    GESTURE_NONE        = 0x00,
    GESTURE_DOWN        = 0x01,
    GESTURE_UP          = 0x02,
    GESTURE_LEFT        = 0x03,
    GESTURE_RIGHT       = 0x04,
    GESTURE_CLICK       = 0x05,
    GESTURE_DOUBLECLICK = 0x0B,
    GESTURE_LONGPRESS   = 0x0C,
} Touch_Gesture_t;

/* ========================== 函数声明 ========================== */

void CST816_Init(void);
uint8_t CST816_GetChipID(void);
uint8_t CST816_GetFingerNum(void);
void CST816_GetTouch(Touch_Info_t* info);
Touch_Gesture_t CST816_GetGesture(void);
void CST816_Sleep(void);
void CST816_Wakeup(void);
void CST816_Test(void);

#endif
