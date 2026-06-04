/**
 * @file    shared_memory.h
 * @brief   全局共享内存数据结构定义
 *
 * @details 整个系统只有一块全局共享内存 g_shm (定义在 shared_memory.c 中)。
 *          它是一块"公共公告板":
 *            - I2CSensTask  在上面写触摸/时间/环境/IMU/心率数据
 *            - PowerTask    在上面写电源数据
 *            - LvglTask     从上面读数据来渲染页面
 *            - DiagTask     从上面读传感器健康状态
 *
 *          设计原则 (设计文档第五节):
 *            规则1: 每个字段只有一个写入者 (Single Writer)
 *            规则2: 任何任务都可以读 (Multiple Reader)
 *            规则3: 写完后用 TaskNotify 通知读者
 *            规则4: 读者不关心是否漏读 (总是读最新值)
 *            规则5: 结构体 32 位对齐, Cortex-M4 对齐访问天然原子
 *
 *          命名规则:
 *            子结构体 → xxx_data_t  (如 touch_data_t)
 *            根结构体 → shared_mem_t
 *            全局实例 → g_shm     (global Shared Memory)
 *
 *          为什么用共享内存而不是队列?
 *            触摸坐标每 5ms 一个 (200Hz), 走队列每次都拷贝一遍太浪费。
 *            共享内存零拷贝 + TaskNotify 通知, 适合高频数据。
 */

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════
 *  1. 触摸数据 — I2CSensTask 写入, LvglTask 读取
 *
 *  x/y 用 int32_t 是为了和 LVGL 的 lv_indev_data_t.point 类型一致,
 *  避免每次赋值都要类型强转。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    int32_t  x;          /* 触摸 X 坐标 (0 ~ 239)                        */
    int32_t  y;          /* 触摸 Y 坐标 (0 ~ 279)                        */
    uint8_t  pressed;    /* 0 = 手指已抬起,  1 = 手指正按着              */
    uint8_t  gesture;    /* 手势寄存器原始值 (读自 I2CSensTask)          */
} touch_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  2. 时间数据 — 来自 DS3231 RTC 芯片 (I2C 地址 0x68)
 *
 *  year 用 uint16_t 是因为年份 (如 2026) 超过了 uint8_t 的范围 (0~255)。
 *  其他字段 (时/分/秒/日/月) 都在 uint8_t 范围内, 用最小够用类型节省 RAM。
 *
 *  last_update_tick: 记录上次从 DS3231 读取的时刻 (HAL_GetTick 值),
 *                    用于后续判断数据是否过期。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    uint8_t  hour;              /* 时 (0~23)                              */
    uint8_t  min;               /* 分 (0~59)                              */
    uint8_t  sec;               /* 秒 (0~59)                              */
    uint8_t  day;               /* 日 (1~31)                              */
    uint8_t  month;             /* 月 (1~12)                              */
    uint8_t  weekday;           /* 星期 (1=周日, 2=周一, ..., 7=周六)    */
    uint16_t year;              /* 年 (如 2026), uint8_t 最大只有255     */
    uint32_t last_update_tick;  /* 最后更新时间戳 (ms)                    */
} time_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  3. 环境数据 — 来自 BME280 传感器 (I2C 地址 0x76)
 *
 *  temperature 和 humidity 放大了 10 倍存储, 避免浮点数。
 *  示例: 22.5℃ → 存储为 225;  55.2% → 存储为 552。
 *  altitude 单位是米 (m), pressure 单位是帕斯卡 (Pa)。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    int16_t  temperature;       /* 温度 × 10 (225 = 22.5℃)              */
    uint16_t humidity;          /* 湿度 × 10 (552 = 55.2%)               */
    uint32_t pressure;          /* 气压 (Pa)                              */
    int16_t  altitude;          /* 海拔 (m), 可为负数                      */
    uint32_t last_update_tick;
} env_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  4. IMU 数据 — 来自 QMI8658 六轴传感器 (I2C 地址 0x6B)
 *
 *  加速度单位 mg, 陀螺仪单位 0.1 dps。
 *  steps 是累计步数 (由 I2CSensTask 内部做计步算法)。
 *  wrist_up: 抬腕检测, 用于唤醒屏幕。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    int16_t  accel_x;           /* 加速度 X 轴 (mg)                       */
    int16_t  accel_y;           /* 加速度 Y 轴 (mg)                       */
    int16_t  accel_z;           /* 加速度 Z 轴 (mg)                       */
    int16_t  gyro_x;            /* 陀螺仪 X 轴 (0.1 dps)                  */
    int16_t  gyro_y;            /* 陀螺仪 Y 轴 (0.1 dps)                  */
    int16_t  gyro_z;            /* 陀螺仪 Z 轴 (0.1 dps)                  */
    uint32_t steps;             /* 累计步数                               */
    uint8_t  wrist_up;          /* 0 = 垂腕,  1 = 抬腕                    */
    uint32_t last_update_tick;
} imu_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  5. 心率血氧数据 — 来自 MAX30102 传感器 (I2C 地址 0x57)
 *
 *  hr_valid / spo2_valid: 传感器可能读失败, 用这两个标志位告诉上层
 *  "这个值有效" 还是 "别用, 还没读到"。UI 据此决定显示数字还是 "--"。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    uint8_t  hr_bpm;            /* 心率值 (次/分钟), 如 72               */
    uint8_t  spo2;              /* 血氧值 (%), 如 98                     */
    uint8_t  hr_valid;          /* 0 = 心率无效,  1 = 心率有效           */
    uint8_t  spo2_valid;        /* 0 = 血氧无效,  1 = 血氧有效           */
    uint32_t last_update_tick;
} hr_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  6. 电源数据 — PowerTask 写入, LvglTask / 状态栏读取
 *
 *  charging: 0=未充电(电池供电), 1=充电中(外部供电)。
 *  backlight: 当前屏幕亮度百分比 (0~100)。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    uint8_t  battery_pct;       /* 电量百分比 (0~100)                     */
    uint8_t  charging;          /* 0 = 未充电,  1 = 充电中               */
    uint8_t  backlight;         /* 当前屏幕亮度 (0~100)                   */
    uint32_t last_update_tick;
} power_data_t;

/* ═══════════════════════════════════════════════════════════════
 *  7. 传感器健康状态 — I2CSensTask 写入, DiagTask / LvglTask 读取
 *
 *  每个传感器占 2 位 (bit-field), 3 种状态:
 *    SENS_OK       (0) → 通信正常
 *    SENS_RETRYING (1) → 正在重试
 *    SENS_FAILED   (2) → 已确认故障, 5 分钟后再试
 *
 *  为什么不直接用 uint8_t 数组?
 *    5 个传感器 × 1 字节 = 5 字节,  用 bit-field → 2 字节, 省 3 字节。
 *    _reserved 是占位用的, 凑满 16 位对齐。
 * ═══════════════════════════════════════════════════════════════ */
#define SENS_OK         0x00    /* 传感器通信正常                          */
#define SENS_RETRYING   0x01    /* 传感器正在重试中                        */
#define SENS_FAILED     0x02    /* 传感器已确认故障                        */

typedef struct {
    uint8_t  touch  : 2;        /* CST816S  触摸                          */
    uint8_t  rtc    : 2;        /* DS3231   RTC 时钟                      */
    uint8_t  env    : 2;        /* BME280   环境                          */
    uint8_t  imu    : 2;        /* QMI8658  六轴                          */
    uint8_t  hr     : 2;        /* MAX30102 心率血氧                      */
    uint8_t  _reserved : 6;     /* 保留位, 凑满 16 位                      */
} sens_health_t;

/* ═══════════════════════════════════════════════════════════════
 *  根结构体 — 把所有子结构体拼成一面完整的"公告板"
 *
 *  __attribute__((aligned(4))):
 *    确保 g_shm 起始地址是 4 的倍数 (32 位对齐)。
 *    Cortex-M4 上对齐的 32 位读写是原子的:
 *      不会出现"读到一半被另一个任务写了"的情况。
 *    这是一种防御性设计——不加大概率也没事, 加了就一定安全。
 *
 *  子结构体中没有 aligned —— 因为外层对齐了,
 *  内部成员会由编译器自动对齐, 不需要每个都标。
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    touch_data_t  touch;        /* 触摸坐标 (每 5ms 更新)                  */
    time_data_t   time;         /* 时间 (每 1s 更新)                      */
    env_data_t    env;          /* 环境 (每 1s 更新)                      */
    imu_data_t    imu;          /* IMU  (每 20ms 更新)                    */
    hr_data_t     hr;           /* 心率 (每 100ms 更新)                   */
    power_data_t  power;        /* 电源 (每 1s 更新)                      */
    sens_health_t sens_health;  /* 传感器健康状态                         */
} __attribute__((aligned(4))) shared_mem_t;

/*
 * extern:  "实物在 shared_memory.c 里, 这里只是声明, 不分配内存"
 * volatile: "这个变量会被多个任务同时读写, 编译器不要做缓存优化,
 *            每次访问都必须从内存重新读取"
 *
 * 谁 include 这个头文件, 谁就能读写 g_shm——
 * 但全系统只有 shared_memory.c 里那唯一一份实例。
 */
extern volatile shared_mem_t g_shm;

#endif
