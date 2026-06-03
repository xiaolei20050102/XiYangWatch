# XiYangWatch 系统架构设计书

> 版本: v4.1  
> 日期: 2026-06-03  
> MCU: STM32F411CE (Cortex-M4F, 96MHz, 512KB Flash, 128KB RAM)  
> GUI: LVGL 9.5 (240×280, RGB565)  
> RTOS: FreeRTOS V10.3.1 (CMSIS-RTOS v2 API)

**v4.1 修订说明:** 修正 v4.0 中任务文件放置于 BSP 目录下的架构偏差。FreeRTOS 任务归属于 Application 层，BSP 为纯硬件驱动函数库。此分层遵循 Zephyr、STM32Cube、NXP MCUXpresso、Nordic nRF5、ARM IoT Reference 等行业标准项目的一致做法。

---

## 目录

1. [设计哲学](#一设计哲学)
2. [分层架构](#二分层架构)
3. [总线与硬件分配](#三总线与硬件分配)
4. [任务全景](#四任务全景)
5. [IPC 通信矩阵](#五ipc-通信矩阵)
6. [共享内存设计](#六共享内存设计)
7. [消息队列设计](#七消息队列设计)
8. [低功耗状态机](#八低功耗状态机)
9. [启动序列](#九启动序列)
10. [故障恢复策略](#十故障恢复策略)
11. [看门狗与心跳监控](#十一看门狗与心跳监控)
12. [文件目录结构](#十二文件目录结构)
13. [编码规范](#十三编码规范)
14. [Phase 2 实施路线图](#十四phase-2-实施路线图)
15. [文件清册](#十五文件清册)
16. [BSP 驱动 API 模板](#十六bsp-驱动-api-模板)

---

## 一、设计哲学

### 1.1 核心原则

| 原则 | 含义 | 落地方式 |
|------|------|---------|
| **单向依赖** | 上层调下层，禁止反向 | App → BSP → HAL。BSP 绝不 include FreeRTOS.h |
| **任务与驱动分离** | 任务决定"何时做"，驱动提供"能做什么" | 所有 FreeRTOS 任务在 App 层，BSP 只导出函数 |
| **零拷贝高频** | 高频数据不经过队列，走共享内存 | g_touch / g_sensor 直接读写，TaskNotify 通知 |
| **不可丢失低频** | 低频事件走消息队列，保证送达 | BLE 消息 / 存储请求 / 电源事件 走 Queue |
| **故障隔离** | 一个模块挂不影响系统运行 | 传感器降级、任务心跳超限软复位 |
| **可观测性** | 运行时状态透明，故障可追溯 | DiagTask 心跳监控 + SEGGER_RTT 日志输出 |
| **确定性** | 所有任务周期和优先级预先设计，不靠"运气"调度 | 高优先级抢占，低优先级填空 |

### 1.2 禁止事项

| 禁止 | 原因 |
|------|------|
| ❌ 任务文件放在 BSP 目录下 | BSP 是纯驱动层，不含 FreeRTOS 概念 |
| ❌ BSP 文件 include FreeRTOS.h | 驱动不依赖操作系统，可脱离 RTOS 独立测试 |
| ❌ HAL 调用不检查返回值 | 静默失败是最难排查的 bug |
| ❌ 任务内 `while(1)` 无阻塞点 | CPU 100% 白转，功耗爆炸 |
| ❌ 跨任务直接调用函数 | 破坏解耦，不知道谁依赖谁 |
| ❌ 全局变量满天飞 | 数据流不可追踪 |
| ❌ 驱动层调应用层函数 | 层次颠倒 |
| ❌ `HAL_Delay()` 在任务中用 | 占用 CPU，阻止低功耗 |
| ❌ malloc/free 在运行时用 | 内存碎片，嵌入式大忌 |

### 1.3 允许的通信方式

```
✅ 共享内存 + TaskNotify  (高频数据, 单写单读)
✅ 消息队列               (低频事件, 多对一, 不可丢失)
✅ Event Group            (多任务同步, 广播)
✅ Software Timer 回调    (定时刷新, 不需独立任务栈)
```

---

## 二、分层架构

### 2.1 五层模型

本项目遵循嵌入式行业标准的 5 层单向依赖模型。此模型被 Zephyr RTOS (Linux 基金会)、STM32Cube 社区、NXP MCUXpresso SDK、Nordic nRF Connect SDK 及 ARM IoT 参考集成所采用。

```
┌──────────────────────────────────────────────────────────────┐
│  Layer 5: Application (App/)                                  │
│  FreeRTOS 任务、业务逻辑、状态机、页面 UI                       │
│  ┌──────────────────────────────────────────────────────────┐│
│  │ 仅调用 BSP 层函数。绝不直接触摸 HAL 寄存器。               ││
│  │ 任务文件: diag_task, power_task, i2c_sens_task,           ││
│  │           bt_task, save_task, lvgl_task                   ││
│  │ 基础设施: shared_memory, ipc_defs, heartbeat, log_port    ││
│  │ 页面管理: page_manager, gesture                           ││
│  │ 数据适配: data_provider                                   ││
│  │ 页面: page_watchface, page_heartrate, ...                 ││
│  └──────────────────────────────────────────────────────────┘│
│                          ↓ 调用                                │
├──────────────────────────────────────────────────────────────┤
│  Layer 4: BSP — Board Support Package (BSP/)                  │
│  纯硬件驱动函数。ZERO FreeRTOS 依赖。ZERO 任务文件。           │
│  ┌──────────────────────────────────────────────────────────┐│
│  │ 每个外设导出: init / read / sleep / wake                  ││
│  │ 不含 FreeRTOS.h。不含 xTaskCreate。不含任务循环。         ││
│  │ 不含 IPC 对象 (队列、信号量、g_shm 引用)。                 ││
│  │ LCD: lcd_st7789    Touch: touch_cst816s                   ││
│  │ RTC: ds3231        ENV: bme280                            ││
│  │ IMU: qmi8658       HR: max30102                           ││
│  │ Flash: w25q64      BLE: esp32_at_hal                      ││
│  └──────────────────────────────────────────────────────────┘│
│                          ↓ 调用                                │
├──────────────────────────────────────────────────────────────┤
│  Layer 3: Drivers (Drivers/)                                   │
│  STM32 HAL + CMSIS。芯片厂商提供，只读。                       │
├──────────────────────────────────────────────────────────────┤
│  Layer 2: Middleware (Middlewares/)                             │
│  第三方库: FreeRTOS V10.3.1, LVGL 9.5, SEGGER_RTT             │
├──────────────────────────────────────────────────────────────┤
│  Layer 1: Hardware (STM32F411CE)                               │
│  Cortex-M4F, 96MHz, 512KB Flash, 128KB SRAM                   │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 黄金法则

```
┌───────────────────────────────────────────────────────────────┐
│ ① 任务文件绝不出现在 BSP 目录下。                              │
│ ② BSP 文件绝不 #include FreeRTOS.h 或其他 RTOS 头文件。        │
│ ③ BSP 输出函数，App 调用它们。依赖方向: App → BSP → HAL。     │
│ ④ 永不反向: BSP 不能调 App 层函数，不能读写 g_shm。           │
│ ⑤ 换 MCU 只需重写 BSP + Drivers，App 层代码不动。             │
└───────────────────────────────────────────────────────────────┘
```

### 2.3 本项目的实际分层

| 目录 | 层 | 内容 | RTOS 依赖 |
|------|:--:|------|:--:|
| `App/Framework/` | App | 所有 FreeRTOS 任务 + IPC 基础设施 | ✅ |
| `App/Pages/` | App | UI 页面 (调 LVGL + data_provider) | ❌ |
| `App/Data/` | App | 数据适配 (读 g_shm) | ❌ |
| `BSP/` | BSP | 纯外设驱动函数库 | ❌ |
| `Drivers/` | HAL | STM32F4 HAL 库 (只读) | ❌ |
| `Middlewares/` | Middleware | FreeRTOS, LVGL, SEGGER_RTT | ✅ |
| `Core/` | 初始化 | CubeMX 生成, main.c, freertos.c | ✅ |

### 2.4 行业参考

| 项目 | App 层 (任务所在) | BSP 层 (纯驱动) |
|------|-------------------|-----------------|
| **Zephyr RTOS** | `src/` (app threads) | `drivers/` (sensor drivers) |
| **STM32Cube 社区** | `App/Tasks/` | `Drivers/BSP/` |
| **NXP MCUXpresso** | `source/` (main + tasks) | `board/` (board init) |
| **Nordic nRF5 SDK** | `examples/` (user main) | `modules/` (nrfx drivers) |
| **ARM IoT Reference** | `applications/` (task files) | `bsp/` (board abstraction) |
| **OV_Watch (参考)** | `User/Tasks/` (13 任务) | `BSP/` (芯片名/ 纯驱动) |

---

## 三、总线与硬件分配

```
                    STM32F411CE
                         │
    ┌────────────────────┼────────────────────────┐
    │                    │                        │
  SPI1                I2C1                     USART1
  PA5/PA6/PA7         PB6(SCL)/PB7(SDA)        PA9(TX)/PA10(RX)
    │                    │                        │
    ├─ ST7789 LCD        ├─ CST816S  0x15 (触摸)   └─ ESP32-C3 (BLE)
    │  CS=PB1            ├─ DS3231   0x68 (时钟)
    │  DC=PB0            ├─ BME280   0x76 (环境)
    │  RST=PB12          ├─ QMI8658  0x6B (IMU)
    │  BL=PWM TIM2_CH2   └─ MAX30102 0x57 (心率)
    │
  SPI2                     ADC                     GPIO
  PB13/PB14/PB15           PA0                     PB2=CST816 INT
    │                      (电池电压)               PB8=CST816 RST
    └─ W25Q64 Flash                                 PA0=充电检测
       CS=PA4
```

### 总线独占分配

| 总线 | 独占任务 | 用途 | 冲突风险 |
|------|---------|------|:--:|
| **SPI1** | LvglTask | LCD DMA 刷屏 | 无 |
| **I2C1** | I2CSensTask | 5 传感器 | 无 |
| **USART1** | BtTask | BLE AT 协议 | 无 |
| **SPI2** | SaveTask | W25Q64 读写 | 无 |
| **ADC** | PowerTask | 电池电压 | 无 |

**物理隔离 = 零互斥锁。这是架构最核心的设计决策。**

---

## 四、任务全景

```
┌──────────────────────────────────────────────────────────────────┐
│                        FreeRTOS Scheduler                         │
│                                                                    │
│  优先级: High ────────────────────────────────────────→ Low       │
│                                                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────────┐ │
│  │LvglTask  │ │I2CSensTask│ │ BtTask   │ │ SaveTask             │ │
│  │ prio:12  │ │ prio:12  │ │ prio:10  │ │ prio:6               │ │
│  │ 8KB      │ │ 2KB      │ │ 4KB      │ │ 2KB                  │ │
│  │ 5ms      │ │ 5ms      │ │事件驱动   │ │ 队列驱动              │ │
│  │ 文件:    │ │ 文件:    │ │ 文件:    │ │ 文件:                │ │
│  │ Framework│ │ Framework│ │ Framework│ │ Framework             │ │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └──────┬───────────────┘ │
│       │            │            │              │                  │
│  ┌────┴────────────┴────────────┴──────────────┴───────────────┐ │
│  │  PowerTask (prio:10, 1KB, 1s)    文件: App/Framework/         │ │
│  │  电池/充电/亮暗屏/休眠仲裁                                     │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  DiagTask (prio:2, 1KB, 1s)       文件: App/Framework/        │ │
│  │  看门狗/心跳监控/异常复位/内存统计/RTT 日志                     │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┘ │
│  │  Tmr SvcTask (FreeRTOS 内建, 系统优先级)                        │
│  │  软件定时器回调执行                                              │
│  └──────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### 任务详细规格

| 属性 | LvglTask | I2CSensTask | BtTask | SaveTask | PowerTask | DiagTask |
|------|----------|-------------|--------|----------|-----------|----------|
| **文件位置** | `App/Framework/lvgl_task.c` | `App/Framework/i2c_sens_task.c` | `App/Framework/bt_task.c` | `App/Framework/save_task.c` | `App/Framework/power_task.c` | `App/Framework/diag_task.c` |
| **优先级** | osPriorityHigh (12) | osPriorityHigh (12) | osPriorityNormal (10) | osPriorityLow (6) | osPriorityNormal (10) | osPriorityLow (2) |
| **栈大小** | 8KB (512×16) | 2KB (128×16) | 4KB (256×16) | 2KB (128×16) | 1KB (64×16) | 1KB (64×16) |
| **周期** | 5ms | 5ms | 事件驱动 | 队列驱动 | 1s | 1s |
| **控制硬件** | 通过 BSP 调 SPI1 | 通过 BSP 调 I2C1 | 通过 BSP 调 USART1 | 通过 BSP 调 SPI2 | ADC+GPIO | UART2+IWDG |
| **核心职责** | LVGL 渲染+PageMgr | 5 传感器采集 | BLE 协议栈 | Flash 持久化 | 电源管理 | 系统监控 |

### 为什么 LvglTask 和 I2CSensTask 同优先级

两者都 5ms 周期，优先级相同。FreeRTOS 时间片轮转会交替执行：
- I2CSensTask 读完触摸写入 `g_touch`，下一个时间片 LvglTask 消费
- 同优先级保证了谁都不会抢占对方导致实时性抖动
- 如果 I2CSensTask 设更高优先级，它会在 LVGL 渲染中途打断导致帧撕裂

---

## 五、IPC 通信矩阵

### 5.1 完整通信图

```
发送→接收    LvglTask       I2CSensTask    BtTask         PowerTask      SaveTask
─────────────────────────────────────────────────────────────────────────────────
I2CSensTask  ▌Notify(触摸)   ▌—           ▌—            ▌—            ▌Queue
             ▌Notify(传感)   ▌            ▌             ▌             ▌(存储请求)
             ▌SharedMem     ▌            ▌             ▌             ▌
─────────────────────────────────────────────────────────────────────────────────
BtTask       ▌Queue(BLE消息) ▌—           ▌—            ▌Notify       ▌Queue
             ▌              ▌            ▌             ▌(消息达)      ▌(OTA数据)
─────────────────────────────────────────────────────────────────────────────────
PowerTask    ▌Queue(低电)   ▌Notify      ▌Notify       ▌—             ▌—
             ▌Queue(亮暗)   ▌(休眠)      ▌(休眠)       ▌              ▌
─────────────────────────────────────────────────────────────────────────────────
LvglTask     ▌—             ▌Notify      ▌Queue        ▌Notify        ▌—
             ▌              ▌(传感器请求) ▌(发送消息)   ▌(用户活跃)     ▌
─────────────────────────────────────────────────────────────────────────────────
SaveTask     ▌Notify        ▌—           ▌—            ▌—             ▌—
             ▌(存储完成)    ▌            ▌             ▌              ▌
```

### 5.2 每条通道的代码定义

```c
// ============================================================
// ipc_defs.h — 所有 IPC 对象统一定义于此 (App/Framework/)
// ============================================================

// ——— Task Notifications (高频, 零拷贝) ———
// 只用作二进制通知(无值传递), 值走共享内存

// ——— Message Queues (低频, 不可丢失) ———
extern osMessageQueueId_t g_q_ble_msg;        // BtTask → LvglTask, BLE消息
extern osMessageQueueId_t g_q_save_req;       // I2CSensTask/BtTask → SaveTask, 存储请求
extern osMessageQueueId_t g_q_power_event;    // PowerTask → LvglTask, 电源事件

// ——— Event Groups (多任务同步) ———
extern osEventFlagsId_t g_ef_sleep;    // PowerTask → 全员, 休眠协调
//   bit0: EVT_PREPARE_SLEEP   休眠准备
//   bit1: ACK_LVGL_READY      LvglTask 就绪
//   bit2: ACK_I2C_READY       I2CSensTask 就绪
//   bit3: ACK_BT_READY        BtTask 就绪
//   bit4: ACK_SAVE_READY      SaveTask 就绪
//   bit5: ACK_DIAG_READY      DiagTask 就绪
//   bit6: EVT_WAKEUP          系统唤醒

extern osEventFlagsId_t g_ef_sys_state; // 系统状态标志
//   bit0: SYS_READY           所有任务就绪
//   bit1: SYS_OTA_MODE        OTA 模式
//   bit2: SYS_SAFE_MODE       安全模式
```

### 5.3 为什么高频不走队列

```
触摸坐标: 每 5ms 一个 (200Hz)
  走队列: 每次 osMessageQueuePut + osMessageQueueGet, 拷贝 12 字节, FreeRTOS 内部临界区
          → 200 次/秒 × 12 字节 × 2 = 4.8KB/秒 无效拷贝

  走共享内存: I2CSensTask 写 12 字节到固定地址
             LvglTask 读 12 字节
             → 零拷贝, 零延迟, 零临界区
             
  配合 TaskNotify: 一个无值的通知告知"新数据到了"
           32-bit 原子操作, 比信号量快 10 倍
```

---

## 六、共享内存设计

### 6.1 数据结构

```c
// ============================================================
// shared_memory.h — 所有全局共享数据统一定义于此 (App/Framework/)
// ============================================================

#include <stdint.h>
#include <stdbool.h>

/* ——— 触摸数据 (I2CSensTask→写, LvglTask→读) ——— */
typedef struct {
    int32_t  x;
    int32_t  y;
    uint8_t  pressed;    // 0=释放, 1=按下
} touch_data_t;

/* ——— 时间数据 (I2CSensTask→写, LvglTask→读) ——— */
typedef struct {
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    uint32_t last_update_tick;
} time_data_t;

/* ——— 环境数据 (I2CSensTask→写, LvglTask→读) ——— */
typedef struct {
    int16_t  temperature;   // ℃ × 10 (225 = 22.5℃)
    uint16_t humidity;       // % × 10
    uint32_t pressure;       // Pa
    int16_t  altitude;       // m
    uint32_t last_update_tick;
} env_data_t;

/* ——— IMU 数据 (I2CSensTask→写, LvglTask→读) ——— */
typedef struct {
    int16_t  accel_x;        // mg
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;         // 0.1 dps
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint32_t steps;          // 累计步数
    uint8_t  wrist_up;       // 0=垂腕, 1=抬腕
    uint32_t last_update_tick;
} imu_data_t;

/* ——— 心率数据 (I2CSensTask→写, LvglTask→读) ——— */
typedef struct {
    uint8_t  hr_bpm;         // 心率值
    uint8_t  spo2;           // 血氧值 (0=无效)
    uint8_t  hr_valid;       // 0=无效, 1=有效
    uint8_t  spo2_valid;
    uint32_t last_update_tick;
} hr_data_t;

/* ——— 电源数据 (PowerTask→写, LvglTask→读) ——— */
typedef struct {
    uint8_t  battery_pct;    // 电量百分比 0~100
    uint8_t  charging;       // 0=未充电, 1=充电中
    uint8_t  backlight;      // 当前亮度 0~100
    uint32_t last_update_tick;
} power_data_t;

/* ——— 传感器状态位 (I2CSensTask→写, DiagTask/LvglTask→读) ——— */
#define SENS_OK         0x00
#define SENS_RETRYING   0x01
#define SENS_FAILED     0x02

typedef struct {
    uint8_t  touch  : 2;    // CST816 状态
    uint8_t  rtc    : 2;    // DS3231 状态
    uint8_t  env    : 2;    // BME280 状态
    uint8_t  imu    : 2;    // QMI8658 状态
    uint8_t  hr     : 2;    // MAX30102 状态
    uint8_t  _reserved : 6;
} sens_health_t;

/* ——— 全局共享内存根结构体 ——— */
typedef struct {
    touch_data_t  touch;
    time_data_t   time;
    env_data_t    env;
    imu_data_t    imu;
    hr_data_t     hr;
    power_data_t  power;
    sens_health_t sens_health;
    // 对齐到 32 位边界，确保原子访问
} __attribute__((aligned(4))) shared_mem_t;

extern volatile shared_mem_t g_shm;
```

### 6.2 访问规则

```
规则 1: 每个字段只有一个写入者 (Single Writer)
规则 2: 任何任务可以读 (Multiple Reader)
规则 3: 写完后用 TaskNotify 通知读者
规则 4: 读者不关心是否漏读 (覆盖无妨, 总是读最新值)
规则 5: 结构体 32 位对齐, Cortex-M4 对齐访问天然原子

写入示例 (I2CSensTask 中):
    g_shm.touch.x = raw_x;
    g_shm.touch.y = raw_y;
    g_shm.touch.pressed = 1;
    __DSB();  // 数据同步屏障, 确保写入完成
    osThreadFlagsSet(lvglTaskHandle, 0x01);

读取示例 (LvglTask 中):
    touch_data_t t;
    t.x = g_shm.touch.x;  // 直接读, 无锁
    t.y = g_shm.touch.y;
```

---

## 七、消息队列设计

### 7.1 队列定义

```c
// ============================================================
// ipc_defs.h — 消息类型枚举 (App/Framework/)
// ============================================================

/* ——— BLE 消息 (BtTask → LvglTask) ——— */
typedef enum {
    BLE_MSG_NOTIFICATION = 0,  // 手机通知
    BLE_MSG_SYNC_DATA,         // 数据同步请求
    BLE_MSG_TIME_SYNC,         // 时间校准
    BLE_MSG_OTA_START,         // OTA 开始
    BLE_MSG_OTA_CHUNK,         // OTA 数据块
    BLE_MSG_OTA_END,           // OTA 完成
    BLE_MSG_CMD_RESP,          // AT 命令响应
} ble_msg_type_t;

typedef struct {
    ble_msg_type_t type;
    uint8_t  payload[256];
    uint16_t payload_len;
} ble_msg_t;

// 队列: g_q_ble_msg, 深度 4, 元素大小 = sizeof(ble_msg_t)

/* ——— 存储请求 (I2CSensTask/BtTask → SaveTask) ——— */
typedef enum {
    SAVE_REQ_HISTORY = 0,      // 运动/心率历史
    SAVE_REQ_CONFIG,           // 系统配置
    SAVE_REQ_OTA_CHUNK,        // OTA 固件块
} save_req_type_t;

typedef struct {
    save_req_type_t type;
    uint32_t flash_addr;
    const uint8_t *data;
    uint16_t data_len;
    uint8_t  callback_needed;  // 写完后是否需要通知
} save_req_t;

// 队列: g_q_save_req, 深度 4, 元素大小 = sizeof(save_req_t)

/* ——— 电源事件 (PowerTask → LvglTask) ——— */
typedef enum {
    PWR_EVT_LOW_BATTERY = 0,  // 低电量警告
    PWR_EVT_CHARGING,          // 开始充电
    PWR_EVT_UNPLUGGED,         // 拔出充电器
    PWR_EVT_BRIGHTNESS_CHG,    // 亮度改变
} power_evt_type_t;

typedef struct {
    power_evt_type_t type;
    uint8_t value;             // 亮度值/电量值
} power_evt_t;

// 队列: g_q_power_event, 深度 4, 元素大小 = sizeof(power_evt_t)
```

### 7.2 队列使用模式

```c
// 生产者 (例如 BtTask 发 BLE 通知消息)
ble_msg_t msg;
msg.type = BLE_MSG_NOTIFICATION;
msg.payload_len = len;
memcpy(msg.payload, data, len);

BaseType_t rc = xQueueSend(g_q_ble_msg, &msg, 100);
if (rc != osOK) {
    // 队列满 → 丢弃最老的消息, 插入新的
    ble_msg_t old;
    xQueueReceive(g_q_ble_msg, &old, 0);  // 非阻塞弹出旧消息
    xQueueSend(g_q_ble_msg, &msg, 0);     // 插入新消息
}

// 消费者 (LvglTask 收 BLE 消息)
ble_msg_t msg;
while (xQueueReceive(g_q_ble_msg, &msg, 0) == pdPASS) {
    // 一次处理完队列中所有消息, 避免积压
    lvgl_handle_ble_msg(&msg);
}
```

---

## 八、低功耗状态机

### 8.1 状态定义

```c
typedef enum {
    PWR_STATE_ACTIVE = 0,   // 全速: LCD 亮, 全传感器, BLE 活跃
    PWR_STATE_IDLE,         // 暗屏: LCD 最低亮, 传感器降频, BLE 正常
    PWR_STATE_STOP,         // 休眠: CPU STOP, 仅 RTC+触摸中断活着
    _PWR_STATE_COUNT
} power_state_t;
```

### 8.2 状态转换

```
            触摸/按键/BLE/充电
    ┌────────────────────────────────────────────┐
    │                                            │
    ▼                                            │
┌─────────┐  无触摸>15s   ┌─────────┐  无触摸>60s   ┌─────────┐
│ ACTIVE  │ ────────────→ │  IDLE   │ ────────────→ │  STOP   │
│ 全速    │ ←──────────── │  暗屏   │ ←──────────── │  休眠   │
└─────────┘  触摸/按键     └─────────┘  触摸/按键     └─────────┘
    ▲                         ▲                         │
    │                         │                         │
    └────── 充电插入 ─────────┘    RTC 500ms 微醒 ──────┘
                                   (喂狗+检查触摸/充电)
```

### 8.3 PowerTask 核心逻辑

```c
// App/Framework/power_task.c
void PowerTask(void *pvParameters)
{
    power_state_t state = PWR_STATE_ACTIVE;
    uint32_t last_user_event = HAL_GetTick();

    while (1)
    {
        // ① 读电池 (通过调用 BSP/ 中的 ADC 封装函数)
        g_shm.power.battery_pct = adc_to_pct(read_battery_adc());
        g_shm.power.charging = charge_detect();

        // ② 亮暗屏逻辑
        uint32_t idle_ms = HAL_GetTick() - last_user_event;

        if (g_shm.power.charging) {
            state = PWR_STATE_ACTIVE;   // 充电不息屏
        } else if (idle_ms < 15000) {
            state = PWR_STATE_ACTIVE;
        } else if (idle_ms < 60000) {
            state = PWR_STATE_IDLE;
        } else {
            state = PWR_STATE_STOP;
        }

        // ③ 执行状态
        switch (state) {
        case PWR_STATE_ACTIVE:
            lcd_set_backlight(user_brightness);
            break;

        case PWR_STATE_IDLE:
            lcd_set_backlight(5);  // 最低亮度
            break;

        case PWR_STATE_STOP:
            enter_stop_mode();  // 见 8.4
            last_user_event = HAL_GetTick();
            state = PWR_STATE_ACTIVE;
            break;
        }

        osDelay(1000);
    }
}
```

### 8.4 STOP 模式进入/退出

```c
static void enter_stop_mode(void)
{
    // ① 广播休眠准备
    osEventFlagsSet(g_ef_sleep, EVT_PREPARE_SLEEP);

    // ② 等待所有任务就绪 (超时 2s, 强制继续)
    uint32_t ack = osEventFlagsWait(
        g_ef_sleep,
        ACK_LVGL_READY | ACK_I2C_READY | ACK_BT_READY |
        ACK_SAVE_READY | ACK_DIAG_READY,
        osFlagsWaitAll | osFlagsNoClear,
        2000
    );

    // ③ 进入 STOP
    vTaskSuspendAll();
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);     // 关背光
        HAL_SPI_DeInit(&hspi1);                       // 关 SPI1
        __HAL_RCC_PWR_CLK_ENABLE();
        HAL_IWDG_Refresh(&hiwdg);                     // 最后喂狗
        CLEAR_BIT(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk);

        // 设 RTC 500ms 闹钟
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 500, RTC_WAKEUPCLOCK_RTCCLK_DIV16);

        HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
        // ← 芯片在这里休眠
        // ← 被 RTC 闹钟 / 触摸 INT / 充电 GPIO 唤醒

        // 恢复
        SET_BIT(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk);
        SystemClock_Config();          // 恢复 96MHz
        HAL_IWDG_Refresh(&hiwdg);      // 醒来第一件事: 喂狗
    }
    xTaskResumeAll();

    // ④ 广播唤醒
    osEventFlagsSet(g_ef_sleep, EVT_WAKEUP);

    // ⑤ 恢复外设
    HAL_SPI_Init(&hspi1);
    lcd_init_light();
    cst816_wakeup();   // 调用 BSP/Touch/touch_cst816s.c 中的函数
}
```

### 8.5 传感器降频表

| 传感器 | ACTIVE | IDLE | STOP |
|--------|:------:|:----:|:----:|
| CST816 (触摸) | 5ms | 20ms | RTC 500ms 微检 |
| QMI8658 (IMU) | 20ms | 100ms | 停 |
| MAX30102 (心率) | 100ms | 停 | 停 |
| DS3231 (RTC) | 1s | 1s | 停 |
| BME280 (环境) | 1s | 停 | 停 |

---

## 九、启动序列

```
上电复位
  │
  ▼
Phase 0: main() — 硬件初始化 (Core/Src/main.c, 跑在特权模式)
  ├── HAL_Init()
  ├── SystemClock_Config() — HSE 25MHz → PLL → 96MHz
  ├── GPIO / DMA / NVIC 初始化
  ├── 外设时钟使能: SPI1 / SPI2 / I2C1 / USART1 / USART2 / ADC
  ├── ST7789 复位 — 硬件复位 10ms → 高 100ms → 背光 80%
  ├── W25Q64 检测 — 读 ChipID
  └── 打印启动横幅: "XiYangWatch v1.0"
         │
         ▼
Phase 1: freertos.c — 内核 + IPC 创建
  ├── osKernelInitialize()
  ├── 创建 IPC 对象:
  │     ├── g_q_ble_msg         = osMessageQueueNew(4, sizeof(ble_msg_t), NULL)
  │     ├── g_q_save_req        = osMessageQueueNew(4, sizeof(save_req_t), NULL)
  │     ├── g_q_power_event     = osMessageQueueNew(4, sizeof(power_evt_t), NULL)
  │     ├── g_ef_sleep          = osEventFlagsNew(NULL)
  │     └── g_ef_sys_state      = osEventFlagsNew(NULL)
  ├── 创建 6 任务 (全部挂起: osThreadNew 默认挂起或创建后 vTaskSuspend)
  │     ├── LvglTask      (文件: App/Framework/lvgl_task.c)
  │     ├── I2CSensTask   (文件: App/Framework/i2c_sens_task.c)
  │     ├── BtTask        (文件: App/Framework/bt_task.c, Phase 3)
  │     ├── SaveTask      (文件: App/Framework/save_task.c, Phase 4)
  │     ├── PowerTask     (文件: App/Framework/power_task.c)
  │     └── DiagTask      (文件: App/Framework/diag_task.c)
  └── osKernelStart()
         │
         ▼
Phase 2: 每个任务的 Self-Test
  各任务自行初始化, 完成后向 DiagTask 注册心跳:
  
  LvglTask (App/Framework/lvgl_task.c):
    ├── lv_init() → lv_port_disp_init() → lv_port_indev_init()
    ├── w25q64_fs_init() (如需要)
    ├── app_init() → PageManager 创建首页
    └── heartbeat_register(TASK_LVGL, 1000)
  
  I2CSensTask (App/Framework/i2c_sens_task.c):
    ├── 调用 BSP 驱动: CST816_Init() (BSP/Touch/)
    ├── I2C 总线扫描 0x01~0x7F → 记录已连接设备
    ├── 逐个初始化在线传感器 (调 BSP/RTC/, BSP/ENV/, BSP/IMU/, BSP/HR/)
    ├── 标记离线传感器为 SENS_FAILED
    └── heartbeat_register(TASK_I2CSENS, 200)
    ※ 此任务不直接操作 I2C 寄存器 — 它调用 BSP 层函数。
  
  PowerTask (App/Framework/power_task.c):
    ├── ADC 校准
    ├── 读初始电池电压
    └── heartbeat_register(TASK_POWER, 3000)
  
  DiagTask (App/Framework/diag_task.c):
    ├── 读 RCC_GetResetFlags() → 记录复位原因
    ├── RTT 输出系统信息
    └── 等待所有任务首轮心跳
         │
         ▼
Phase 3: Ready → Run
  ├── DiagTask: 所有任务首轮心跳 OK → eg_sys_state |= SYS_READY
  └── 正常运行
```

---

## 十、故障恢复策略

### 10.1 故障分级

| 级别 | 现象 | 恢复策略 |
|:--:|------|---------|
| **L1** | 单个传感器通信失败 | 标记 SENS_FAILED, UI 显示"--", 其他传感器正常工作 |
| **L2** | 传感器连续重试 3 次仍失败 | 标记 SENS_FAILED, 5 分钟后尝试恢复 |
| **L3** | I2C 总线完全不通(4 个以上传感器 FAILED) | I2C1 复位 + 重新初始化, 仍不通 → 软复位 |
| **L4** | 任务心跳超时 | DiagTask 记录 → 软复位 |
| **L5** | 软复位 3 次内重演 | 进入 Safe Mode (仅 表盘+触摸) |
| **L6** | IWDG 复位 | 记录原因 → 正常启动 |

### 10.2 传感器接口抽象

```c
// ============================================================
// sens_driver.h — 传感器驱动统一接口 (App/Framework/)
//
// 注意: 此文件定义的是 APP 层的抽象接口 (虚函数表模式)。
//       具体的 BSP 驱动实现在 BSP/xxx/ 目录下。
//       BSP 文件不知道这个接口存在 — 它们只导出函数。
//       由 I2CSensTask 负责把 BSP 函数指针填入虚函数表。
// ============================================================

typedef enum {
    SENS_INIT_UNINIT = 0,
    SENS_INIT_OK,
    SENS_INIT_FAILED,
} sens_init_result_t;

typedef struct {
    /* 配置 */
    uint8_t   i2c_addr;
    uint16_t  init_retry_max;

    /* 状态 */
    uint8_t   state;          // SENS_OK / SENS_RETRYING / SENS_FAILED
    uint8_t   retry_count;
    uint32_t  last_error_tick;

    /* 虚函数表 — 指向 BSP 层的具体实现
       例如: .init = ds3231_init (BSP/RTC/ds3231.c) */
    sens_init_result_t (*init)(void);
    uint8_t            (*read)(void);       // 返回 HAL_OK 或 HAL_ERROR
    void               (*sleep)(void);
    void               (*wakeup)(void);
} sens_driver_t;

// 宏: 驱动注册 — 在 I2CSensTask 中使用
#define SENS_DEFINE(name, addr) \
    static sens_driver_t sens_##name = { \
        .i2c_addr = addr, \
        .init_retry_max = 3, \
        .init = name##_init, \
        .read = name##_read, \
        .sleep = name##_sleep, \
        .wakeup = name##_wakeup, \
    }
```

### 10.3 传感器读取包装 (带故障恢复)

```c
// I2CSensTask (App/Framework/i2c_sens_task.c) 内部
// 注: 这里调用的 drv->read() 指向 BSP 层函数 (如 bme280_read())
static void sens_read_with_retry(sens_driver_t *drv, uint8_t *health_flag)
{
    if (drv->state == SENS_FAILED) {
        // 5 分钟后尝试恢复
        if (HAL_GetTick() - drv->last_error_tick > 300000) {
            drv->state = SENS_RETRYING;
            drv->retry_count = 0;
        } else {
            return;  // 还没到重试时间
        }
    }

    uint8_t rc = drv->read();   // ← 调 BSP 函数, 不调 HAL

    if (rc == HAL_OK) {
        drv->state = SENS_OK;
        drv->retry_count = 0;
        *health_flag = SENS_OK;
    } else {
        drv->retry_count++;
        if (drv->retry_count >= drv->init_retry_max) {
            drv->state = SENS_FAILED;
            drv->last_error_tick = HAL_GetTick();
            *health_flag = SENS_FAILED;
        } else {
            *health_flag = SENS_RETRYING;
        }
    }
}
```

---

## 十一、看门狗与心跳监控

### 11.1 双层看门狗

```
Layer 1 — IWDG (硬件)
  ├── 时钟: LSI 32kHz, CPU 停了照样跑
  ├── 超时: 2s
  ├── 喂狗: DiagTask 每 1s 喂一次
  └── CPU 真的挂死 → 2s 后硬件复位

Layer 2 — 软件心跳 (应用层)
  ├── 每个任务注册心跳超时值
  ├── DiagTask 每 1s 扫描所有任务心跳
  ├── 超时 → RTT 输出 "DEAD: TaskName (last: Xms)"
  └── → 软复位 (NVIC_SystemReset)
```

### 11.2 心跳 API

```c
// ============================================================
// heartbeat.h — 任务心跳监控 (App/Framework/)
// ============================================================

#define MAX_MONITORED_TASKS  6

typedef enum {
    TASK_LVGL = 0,
    TASK_I2CSENS,
    TASK_BT,
    TASK_SAVE,
    TASK_POWER,
    TASK_DIAG,
} task_id_t;

/* ——— 任务侧 API ——— */
void heartbeat_register(task_id_t id, uint32_t timeout_ms);
void heartbeat_tick(task_id_t id);  // 任务主循环中调用

/* ——— DiagTask 侧 API ——— */
void heartbeat_monitor_all(void);   // DiagTask 每 1s 调用
```

### 11.3 实现

```c
// heartbeat.c (App/Framework/)

typedef struct {
    uint32_t last_tick;
    uint32_t timeout_ms;
    uint8_t  registered;
    uint8_t  dead_count;
} heartbeat_entry_t;

static heartbeat_entry_t g_heartbeats[MAX_MONITORED_TASKS];

void heartbeat_register(task_id_t id, uint32_t timeout_ms)
{
    configASSERT(id < MAX_MONITORED_TASKS);
    g_heartbeats[id].last_tick   = HAL_GetTick();
    g_heartbeats[id].timeout_ms  = timeout_ms;
    g_heartbeats[id].registered  = 1;
    g_heartbeats[id].dead_count  = 0;
}

void heartbeat_tick(task_id_t id)
{
    g_heartbeats[id].last_tick = HAL_GetTick();
}

void heartbeat_monitor_all(void)
{
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        if (!g_heartbeats[i].registered) continue;
        
        if (now - g_heartbeats[i].last_tick > g_heartbeats[i].timeout_ms) {
            g_heartbeats[i].dead_count++;
            log_printf("[DEAD] Task %d timeout (%lu ms, #%lu)\r\n",
                        i, g_heartbeats[i].timeout_ms, g_heartbeats[i].dead_count);
            
            if (g_heartbeats[i].dead_count >= 3) {
                log_printf("[FATAL] Soft reset\r\n");
                HAL_Delay(50);  // 等 RTT 发完
                NVIC_SystemReset();
            }
        } else {
            g_heartbeats[i].dead_count = 0;  // 恢复清零
        }
    }
}
```

---

## 十二、文件目录结构

```
XiYang_Watch/
│
├── Core/                                    ← Layer: HAL Init (CubeMX autogen)
│   ├── Inc/
│   │   ├── FreeRTOSConfig.h                 ← configUSE_TICKLESS_IDLE=1
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── gpio.h / i2c.h / spi.h / ...
│   └── Src/
│       ├── main.c                           ← Phase 0 硬件初始化
│       ├── freertos.c                       ← Phase 1 IPC+任务创建
│       ├── stm32f4xx_it.c                   ← 中断服务
│       ├── i2c.c / spi.c / usart.c / ...    ← CubeMX 外设 Init
│       └── system_stm32f4xx.c
│
├── App/                                     ← Layer 5: Application
│   ├── app.c / app.h                        ← app_init / app_loop
│   │
│   ├── Framework/                           ← 所有 FreeRTOS 任务 + IPC 基础设施
│   │   ├── ipc_defs.h / ipc_defs.c          ← [已有] Queue/EventGroup 声明+定义
│   │   ├── shared_memory.h / shared_memory.c ← [已有] g_shm 结构体+实例
│   │   ├── heartbeat.h / heartbeat.c        ← [已有] 心跳注册/报到/监控
│   │   ├── log_port.h / log_port.c          ← [已有] SEGGER_RTT 日志接口
│   │   ├── sens_driver.h                    ← [待建] 传感器驱动抽象接口
│   │   ├── error_handler.h / error_handler.c← [待建] 错误码 + 故障记录
│   │   │
│   │   ├── lvgl_task.h / lvgl_task.c        ← [待建] LvglTask: LVGL渲染+PageMgr
│   │   ├── diag_task.h / diag_task.c        ← [已有] DiagTask: 心跳+IWDG+日志
│   │   ├── power_task.h / power_task.c      ← [已有] PowerTask: 电池+低功耗
│   │   ├── i2c_sens_task.h / i2c_sens_task.c← [待建] I2CSensTask: 5传感器采集
│   │   ├── bt_task.h / bt_task.c            ← [待建] BtTask: BLE协议栈
│   │   ├── save_task.h / save_task.c        ← [待建] SaveTask: Flash持久化
│   │   │
│   │   ├── page_manager.h / page_manager.c  ← [已有] 页面导航
│   │   ├── gesture.h / gesture.c            ← [已有] 手势识别
│   │   └── status_bar.h / status_bar.c      ← [已有] 状态栏
│   │
│   ├── Data/
│   │   ├── data_provider.h                  ← [已有, 待改为读 g_shm]
│   │   └── data_provider.c                  ← [已有, 待去除 USE_FAKE_DATA]
│   │
│   └── Pages/                               ← [已有, 保持不变]
│       ├── page_watchface.c
│       ├── page_heartrate.c
│       ├── page_control_center.c
│       ├── page_menu.c
│       ├── ...
│       └── pages_config.h / pages_config.c
│
├── BSP/                                     ← Layer 4: 纯硬件驱动 (ZERO FreeRTOS)
│   │
│   ├── LCD/
│   │   ├── lcd_st7789.h                     ← [已有] 需修 include guard
│   │   └── lcd_st7789.c
│   │
│   ├── Touch/
│   │   ├── touch_cst816s.h                  ← [已有] 需修 include guard
│   │   └── touch_cst816s.c
│   │
│   ├── Flash/
│   │   ├── w25q64.h / w25q64.c              ← [已有]
│   │   ├── w25q64_port.h / w25q64_port.c    ← [已有]
│   │   ├── w25q64_fs.h / w25q64_fs.c        ← [已有]
│   │   └── w25q64_program.h / w25q64_program.c ← [已有]
│   │
│   ├── RTC/
│   │   ├── ds3231.h                         ← [待建, Phase 2 Step 5]
│   │   └── ds3231.c                         ← 只导出 init/read/sleep/wake
│   │
│   ├── ENV/
│   │   ├── bme280.h                         ← [待建, Phase 2 Step 6]
│   │   └── bme280.c                         ← 只导出 init/read/sleep/wake
│   │
│   ├── IMU/
│   │   ├── qmi8658.h                        ← [待建, Phase 2 Step 7]
│   │   └── qmi8658.c                        ← 只导出 init/read_accel/read_gyro
│   │
│   ├── HR/
│   │   ├── max30102.h                       ← [待建, Phase 2 Step 8]
│   │   └── max30102.c                       ← 只导出 init/read_fifo/sleep
│   │
│   └── BLE/
│       ├── esp32_at_hal.h                   ← [待建, Phase 3]
│       └── esp32_at_hal.c                   ← UART 收发 + AT 命令封装 ONLY
│           ※ NO bt_task.c here! 任务逻辑在 App/Framework/bt_task.c
│
├── Drivers/                                 ← Layer 3: STM32 HAL (只读)
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
│
├── Middlewares/                             ← Layer 2: 第三方库
│   ├── FreeRTOS/
│   ├── LVGL/
│   │   ├── porting/
│   │   │   ├── lv_port_disp.c               ← [已有]
│   │   │   └── lv_port_indev.c              ← [待改: 改为从 g_shm.touch 读]
│   │   └── lv_conf.h
│   └── SEGGER_RTT/
│       ├── SEGGER_RTT.c / SEGGER_RTT.h
│       ├── SEGGER_RTT_Conf.h
│       └── SEGGER_RTT_printf.c
│
├── EIDE/                                    ← IDE 项目配置
│   └── .eide/
│       └── eide.yml
│
└── Docs/
    ├── system_architecture.md               ← 本文档 (v4.1)
    ├── architecture.md                      ← (旧版, 保留参考)
    ├── learning/
    │   ├── 01_ST7789_驱动原理与SPI通信详解.md
    │   ├── 02_CST816S_驱动原理与I2C通信详解.md
    │   └── 03_I2C通信协议详解.md
    └── pin_assignment.md
```

### 为什么任务文件在 App/Framework/ 而不在 BSP/

| 原因 | 说明 |
|------|------|
| **行业标准** | Zephyr: app/ 放线程, drivers/ 放驱动。STM32Cube: Application/ 放任务, BSP/ 放驱动。NXP: source/ 放应用, board/ 放板级。没有任何 RTOS 项目把任务放在 BSP 目录下。 |
| **可移植性** | 从 STM32F411 换到其他 MCU 时，只需重写 BSP + Drivers 层。App/Framework/ 的代码不依赖具体芯片型号。 |
| **可测试性** | 可以 Mock BSP 函数在 PC 上对任务逻辑跑单元测试。如果任务直接调 HAL，就无法脱离硬件测试。 |
| **编译顺序** | BSP 可以脱离 FreeRTOS 编译为独立库，用于硬件 bring-up 测试。如果 BSP 文件 include 了 FreeRTOS.h，就无法独立编译。 |
| **心智模型** | "BSP 回答硬件能做什么 (WHAT)。任务决定什么时候做 (WHEN)。" 简单，可教，不会混淆。 |
| **OV_Watch 参考** | 13 个任务全在 `User/Tasks/`。`BSP/MPU6050/`、`BSP/AHT21/` 只有纯驱动 `.c/.h` 文件。 |

---

## 十三、编码规范

### 13.1 Include Guard 规范

```
❌ 禁止: 双下划线开头或包含双下划线 (C 标准保留给编译器):
    #ifndef __TOUCH_CST816S_H__   ← 禁止!
    #ifndef __LEC_ST7789_H__      ← 禁止! 还有 LEC 拼写错误

✅ 正确: 全大写 + 单下划线 (与已有 Framework 文件一致):
    #ifndef TOUCH_CST816S_H
    #define TOUCH_CST816S_H

    #ifndef HEARTBEAT_H
    #define HEARTBEAT_H

    #ifndef IPC_DEFS_H
    #define IPC_DEFS_H

已有文件需修正 (Phase 2 Step 0):
  - BSP/Touch/touch_cst816s.h:  __TOUCH_CST816S_H__ → TOUCH_CST816S_H  [已完成]
  - BSP/LCD/lcd_st7789.h:       __LEC_ST7789_H__    → LCD_ST7789_H     [已完成]
```

### 13.2 命名规则

```c
// 全局变量: g_ 前缀
volatile shared_mem_t g_shm;
osMessageQueueId_t         g_q_ble_msg;

// 静态变量: s_ 前缀
static uint32_t s_last_tick;
static IWDG_HandleTypeDef s_hiwdg;

// 常量: k_ 前缀 (暂未使用, 留予将来)
static const uint8_t k_i2c_addrs[] = {0x15, 0x68, 0x76, 0x6A, 0x57};

// 宏: 全大写 + 下划线
#define SENSOR_RETRY_MAX    3
#define I2C_TIMEOUT_MS      100

// 类型: snake_case + _t 后缀
typedef struct { ... } touch_data_t;
typedef enum   { ... } power_state_t;

// 函数: snake_case, 动词在前
void sens_read_with_retry(sens_driver_t *drv, uint8_t *flag);
uint8_t cst816_get_finger_num(void);
```

### 13.3 HAL 返回值检查

```c
// ❌ 不允许: 直接调用, 忽略返回值
HAL_I2C_Mem_Read(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);

// ✅ 强制: 检查返回值, 记录错误
HAL_StatusTypeDef rc;
rc = HAL_I2C_Mem_Read(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
if (rc != HAL_OK) {
    error_log(HAL_ERROR_I2C, addr, rc, __LINE__);
    return RC_FAIL;
}
```

### 13.4 任务函数标准模板

```c
// ============================================================
// 模板: App/Framework/ 下的任务文件
// ============================================================

void ExampleTask(void *pvParameters)
{
    // ═══ 阶段 1: 初始化 ═══
    // 调用 BSP 函数初始化硬件 (不直接调 HAL)
    // 分配资源 (用静态或栈变量, 不 malloc)
    
    // ═══ 阶段 2: 自检 ═══
    // 验证硬件是否在线, 通信是否正常
    
    // ═══ 阶段 3: 注册心跳 ═══
    heartbeat_register(TASK_EXAMPLE, 1000);
    
    // ═══ 阶段 4: 主循环 ═══
    while (1)
    {
        // 4a. 接收 IPC 消息 (Queue / Notify / EventGroup)
        // 4b. 处理业务逻辑 (调 BSP 函数读写硬件)
        // 4c. 发送 IPC 消息
        // 4d. 更新心跳
        heartbeat_tick(TASK_EXAMPLE);
        
        // 4e. 阻塞等待 (必须有这一步, 不能空转)
        osDelay(5);
    }
}
```

### 13.5 禁止模式清单

```c
// ❌ 禁止 1: BSP 文件 include FreeRTOS 头文件
// 文件: BSP/IMU/qmi8658.c
#include "FreeRTOS.h"   // ← 禁止! BSP 不依赖 RTOS

// ❌ 禁止 2: 任务文件放在 BSP 目录下
// 错误路径: BSP/I2CSens/i2c_sens_task.c  ← 禁止!
// 正确路径: App/Framework/i2c_sens_task.c ← 正确

// ❌ 禁止 3: 忙等
while (SPI_IS_BUSY()) {}  // CPU 100%

// ❌ 禁止 4: 驱动层调应用层
void cst816_read(void) {
    lvgl_update_ui();  // BSP 不知道 App 的存在
}

// ❌ 禁止 5: 跨任务直接调函数 (应走 IPC)
bt_send_message("hello");  // LvglTask 直接调 BtTask 的函数

// ❌ 禁止 6: HAL_Delay 在 FreeRTOS 任务中 (阻塞调度器)
HAL_Delay(100);  // 应该用 osDelay(100)

// ❌ 禁止 7: 运行时 malloc/free
uint8_t *buf = malloc(256);  // 碎片! 用栈变量或静态缓冲区
```

---

## 十四、Phase 2 实施路线图

> **当作填空题, 按步骤做, 每步做完可独立验证。**

### Step 0: 环境检查 + Include Guard 修正

- [x] 确认 `FreeRTOSConfig.h` 关键配置:
  ```c
  #define configUSE_TICKLESS_IDLE          1
  #define configCHECK_FOR_STACK_OVERFLOW   2
  #define configUSE_TASK_NOTIFICATIONS     1
  #define configTICK_RATE_HZ               1000
  ```
- [x] 确认 UART2 可用 (PA2 TX, 115200)
- [x] 确认 I2C1 工作正常 (用 CST816_Test 验证)
- [x] 修正 Include Guard: `touch_cst816s.h`, `lcd_st7789.h`

### Step 1: IPC 基础设施 (已完成 ✅)

创建文件:
- `App/Framework/ipc_defs.h` + `ipc_defs.c` ✅
- `App/Framework/shared_memory.h` + `shared_memory.c` ✅
- `App/Framework/heartbeat.h` + `heartbeat.c` ✅

### Step 2: DiagTask + SEGGER_RTT (已完成 ✅)

```c
// App/Framework/diag_task.c  — 系统诊断任务
void DiagTask(void *pvParameters)
{
    // 读复位原因
    uint32_t rst_flag = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRSTF);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    
    log_printf("\r\n=== XiYangWatch v1.0 ===\r\n");
    log_printf("Reset: %s\r\n", rst_flag ? "IWDG" : "POR");
    
    heartbeat_register(TASK_DIAG, 3000);
    
    while (1) {
        heartbeat_monitor_all();  // 扫描所有任务
        
        log_printf("[DIAG] HeapFree:%u LvglStackHWM:%u\r\n",
                    xPortGetFreeHeapSize(),
                    osThreadGetStackSpace(lvglTaskHandle));
        log_printf("[DIAG] HeapFree:%u PowerStackHWM:%u\r\n",
                    xPortGetFreeHeapSize(),
                    uxTaskGetStackHighWaterMark(g_power_task_handle));
        
        HAL_IWDG_Refresh(&s_hiwdg);
        heartbeat_tick(TASK_DIAG);
        osDelay(1000);
    }
}
```

验证: RTT Viewer 看到启动横幅 + 内存统计 ✅

### Step 3: PowerTask (已完成 ✅)

```c
// App/Framework/power_task.c  — 电源管理任务
void PowerTask(void *pvParameters)
{
    heartbeat_register(TASK_POWER, 3000);
    
    while (1) {
        // TODO: ADC 读电池 (当前写死 85%)
        g_shm.power.battery_pct = 85;
        
        // TODO: 休眠状态机 (当前只做 ACTIVE)
        
        heartbeat_tick(TASK_POWER);
        osDelay(1000);
    }
}
```

验证: 任务跑起来, 心跳正常 ✅

### Step 4: I2CSensTask — 触摸搬迁 (预计 2h)

这是 Phase 2 最关键的步骤。

**关键架构修正 (v4.1):**
- 任务文件位于 `App/Framework/i2c_sens_task.c` (不是 `BSP/I2CSens/`!)
- 任务函数调用 BSP 层的 `CST816_GetFingerNum()` 等函数
- BSP 文件 (`BSP/Touch/touch_cst816s.c`) 不感知 FreeRTOS、g_shm、TaskNotify

```c
// App/Framework/i2c_sens_task.c — I2CSensTask 主循环
// ============================================================
// 此文件属于 App 层: 使用 FreeRTOS API、读写 g_shm、注册心跳。
// 它调用 BSP 层函数来操作硬件，但不直接调用 HAL。
// ============================================================

#include "cmsis_os.h"
#include "touch_cst816s.h"     // BSP/Touch/  — cst816_get_finger_num()
#include "shared_memory.h"     // App/Framework/ — g_shm
#include "heartbeat.h"         // App/Framework/ — heartbeat_register
#include "log_port.h"          // App/Framework/ — log_printf

extern osThreadId_t lvglTaskHandle;  // 用于 TaskNotify

void I2CSensTask(void *pvParameters)
{
    // ① 触摸初始化 (调 BSP 函数)
    CST816_Init();
    
    // ② I2C 总线扫描 (后面补)
    
    // ③ 注册心跳
    heartbeat_register(TASK_I2CSENS, 200);
    
    uint8_t counter = 0;
    
    while (1)
    {
        // ④ 读触摸 (每周期, 调 BSP 函数)
        if (cst816_get_finger_num() != 0x00 && cst816_get_finger_num() != 0xFF) {
            Touch_Info_t info;
            CST816_GetTouch(&info);
            g_shm.touch.x = info.X_Pos;
            g_shm.touch.y = info.Y_Pos;
            g_shm.touch.pressed = 1;
        } else {
            g_shm.touch.pressed = 0;
        }
        __DSB();
        osThreadFlagsSet(lvglTaskHandle, 0x01);  // 通知 LVGL
        
        // ⑤ 其他传感器 (按频率分档, 后面补)
        
        // ⑥ 心跳 + 阻塞
        heartbeat_tick(TASK_I2CSENS);
        counter++;
        osDelay(5);
    }
}
```

**同时修改 `lv_port_indev.c`**: 改为从 `g_shm.touch` 读, 不再直接调 CST816:

```c
// Middlewares/LVGL/porting/lv_port_indev.c
#include "shared_memory.h"   // App/Framework/

static bool touchpad_is_pressed(void)
{
    return g_shm.touch.pressed;
}

static void touchpad_get_xy(int32_t *x, int32_t *y)
{
    *x = g_shm.touch.x;
    *y = g_shm.touch.y;
}
```

**修改 `freertos.c`**: 创建 I2CSensTask:

```c
// Core/Src/freertos.c
#include "i2c_sens_task.h"    // App/Framework/

osThreadId_t lvglTaskHandle;  // 供 I2CSensTask 用 TaskNotify

// 在 MX_FREERTOS_Init 中:
i2cSensTaskHandle = osThreadNew(I2CSensTask, NULL, &i2cSensTask_attributes);
```

验证: **触摸功能完全正常** — 这是最关键的 checkpoint。如果触摸不行, 回退排查, 不要继续。

### Step 5: DS3231 驱动 (预计 1.5h)

```
BSP/RTC/ds3231.h + ds3231.c

注意: 这是 BSP 驱动文件 — 不含 FreeRTOS.h, 不含任务。
只导出: ds3231_init(), ds3231_read_time(), ds3231_set_time()

和 CST816 套路完全一样:
  ① I2C 地址 0x68
  ② 读 7 个时间寄存器 (秒/分/时/星期/日/月/年)
  ③ 在 I2CSensTask 里每 200 个周期调一次 ds3231_read_time()
  ④ 写 g_shm.time
  ⑤ 表盘不再读假时间
```

验证: RTT 日志打印真实时间

### Step 6: BME280 驱动 (预计 2h)

```
BSP/ENV/bme280.h + bme280.c

BSP 驱动文件 — 不含 FreeRTOS。
导出: bme280_init(), bme280_read(), bme280_sleep(), bme280_wakeup()

  ① I2C 地址 0x76
  ② 初始化: 配置过采样率 / IIR滤波器 / 模式
  ③ 读温度/湿度/气压补偿寄存器
  ④ 套公式算出真实值
  ⑤ I2CSensTask 调用 bme280_read() → 写 g_shm.env
```

验证: RTT 日志显示真实温湿气压

### Step 7: QMI8658 驱动 (预计 2.5h)

```
BSP/IMU/qmi8658.h + qmi8658.c

BSP 驱动文件 — 不含 FreeRTOS。
导出: qmi8658_init(), qmi8658_read_accel(), qmi8658_read_gyro()

  ① I2C 地址 0x6B
  ② 初始化: 配置加速度量程 / 陀螺仪量程 / ODR
  ③ 读加速度 XYZ + 陀螺仪 XYZ
  ④ I2CSensTask 内部做计步累计
  ⑤ 写 g_shm.imu
```

验证: RTT 日志显示步数变化

### Step 8: MAX30102 驱动 (预计 3h)

```
BSP/HR/max30102.h + max30102.c

BSP 驱动文件 — 不含 FreeRTOS。
导出: max30102_init(), max30102_read_fifo(), max30102_sleep()

  ① I2C 地址 0x57
  ② 初始化: 配置 LED 电流 / FIFO / 采样率
  ③ 读 FIFO → 提取红光/红外数据
  ④ 心率算法 (查表或滑动窗口)
  ⑤ I2CSensTask 调 max30102_read_fifo() → 写 g_shm.hr
```

验证: RTT 日志显示真实心率

### Step 9: data_provider 去假数据 (预计 1h)

```c
// App/Data/data_provider.c — 删除 USE_FAKE_DATA, 全部指向 g_shm
#include "shared_memory.h"

uint8_t watch_data_get_heart_rate(void) {
    return g_shm.hr.hr_valid ? g_shm.hr.hr_bpm : 0;
}
uint32_t watch_data_get_steps(void) {
    return g_shm.imu.steps;
}
// ... 等等
```

验证: 所有页面数据来自真实传感器 🔴 **必须接屏幕**

### Step 10: 停机整理

- [ ] 确认所有 HAL 调用检查返回值
- [ ] 确认所有任务 while(1) 有阻塞点
- [ ] 运行 30 分钟, 确认心跳无超时
- [ ] 运行 30 分钟, 确认无内存泄露 (xPortGetFreeHeapSize 稳定)
- [ ] 确认没有任何 BSP 文件 include FreeRTOS.h
- [ ] 所有新代码加 doxygen 注释

### Step 11: LvglTask 提取 (预计 0.5h)

当前 `StartLvglTask()` 函数直接写在 `Core/Src/freertos.c` 中。将其提取到 App 层：

```
App/Framework/lvgl_task.h + lvgl_task.c

void lvgl_task(void *pvParameters);

freertos.c 改为:
  #include "lvgl_task.h"
  lvglTaskHandle = osThreadNew(lvgl_task, NULL, &lvglTask_attributes);
```

---

## 十五、文件清册

### App/Framework/ (Layer 5: Application — Tasks + IPC)

| 文件 | 状态 | Phase | 说明 |
|------|:----:|:-----:|------|
| ipc_defs.h | ✅ | — | IPC 对象声明 (Queue/EventGroup) |
| ipc_defs.c | ✅ | — | IPC 对象定义 |
| shared_memory.h | ✅ | — | g_shm 结构体定义 |
| shared_memory.c | ✅ | — | g_shm 实例 |
| heartbeat.h | ✅ | — | 心跳 API 声明 |
| heartbeat.c | ✅ | — | 心跳监控实现 |
| log_port.h | ✅ | — | 日志接口 (编译期开关) |
| log_port.c | ✅ | — | 日志实现 (SEGGER_RTT) |
| diag_task.h | ✅ | — | DiagTask 入口 |
| diag_task.c | ✅ | — | 诊断任务 |
| power_task.h | ✅ | — | PowerTask 入口 |
| power_task.c | ✅ | — | 电源管理任务 |
| i2c_sens_task.h | 📋 | Step 4 | I2CSensTask 入口 |
| i2c_sens_task.c | 📋 | Step 4 | I2C 传感器采集任务 |
| lvgl_task.h | 📋 | Step 11 | LvglTask 入口 (从 freertos.c 提取) |
| lvgl_task.c | 📋 | Step 11 | LVGL 渲染任务 |
| sens_driver.h | 📋 | Step 4 | 传感器驱动抽象接口 |
| error_handler.h | 📋 | Step 4 | 错误码 + 故障记录 |
| error_handler.c | 📋 | Step 4 | 错误记录实现 |
| bt_task.h | 📋 | Phase 3 | BtTask 入口 |
| bt_task.c | 📋 | Phase 3 | BLE 协议任务 |
| save_task.h | 📋 | Phase 4 | SaveTask 入口 |
| save_task.c | 📋 | Phase 4 | Flash 存储任务 |
| page_manager.h | ✅ | — | 页面导航 |
| page_manager.c | ✅ | — | 页面导航实现 |
| gesture.h | ✅ | — | 手势识别 |
| gesture.c | ✅ | — | 手势识别实现 |
| status_bar.h | ✅ | — | 状态栏 UI |
| status_bar.c | ✅ | — | 状态栏 UI 实现 |

### BSP/ (Layer 4: Board Support Package — Pure Drivers)

| 文件 | 状态 | Phase | 说明 |
|------|:----:|:-----:|------|
| LCD/lcd_st7789.h | ✅ | — | ST7789 驱动 (include guard 已修) |
| LCD/lcd_st7789.c | ✅ | — | ST7789 驱动 |
| Touch/touch_cst816s.h | ✅ | — | CST816S 驱动 (include guard 已修) |
| Touch/touch_cst816s.c | ✅ | — | CST816S 驱动 |
| Flash/w25q64.h | ✅ | — | W25Q64 驱动 |
| Flash/w25q64.c | ✅ | — | W25Q64 驱动 |
| Flash/w25q64_port.h | ✅ | — | SPI 端口绑定 |
| Flash/w25q64_port.c | ✅ | — | SPI 端口绑定 |
| Flash/w25q64_fs.h | ✅ | — | Flash 简易文件系统 |
| Flash/w25q64_fs.c | ✅ | — | Flash 简易文件系统 |
| Flash/w25q64_program.h | ✅ | — | Flash 编程工具 |
| Flash/w25q64_program.c | ✅ | — | Flash 编程工具 |
| RTC/ds3231.h | 📋 | Step 5 | DS3231 纯驱动 |
| RTC/ds3231.c | 📋 | Step 5 | DS3231 纯驱动 |
| ENV/bme280.h | 📋 | Step 6 | BME280 纯驱动 |
| ENV/bme280.c | 📋 | Step 6 | BME280 纯驱动 |
| IMU/qmi8658.h | 📋 | Step 7 | QMI8658 纯驱动 |
| IMU/qmi8658.c | 📋 | Step 7 | QMI8658 纯驱动 |
| HR/max30102.h | 📋 | Step 8 | MAX30102 纯驱动 |
| HR/max30102.c | 📋 | Step 8 | MAX30102 纯驱动 |
| BLE/esp32_at_hal.h | 📋 | Phase 3 | ESP32 AT 命令 HAL 封装 |
| BLE/esp32_at_hal.c | 📋 | Phase 3 | ESP32 AT 命令 HAL 封装 |

### Middlewares/

| 文件 | 状态 | 说明 |
|------|:----:|------|
| LVGL/porting/lv_port_disp.c | ✅ | LVGL 显示移植 |
| LVGL/porting/lv_port_indev.c | 🔧 | 待改为从 g_shm.touch 读 (Step 4) |
| SEGGER_RTT/SEGGER_RTT.c | ✅ | RTT 核心库 |
| SEGGER_RTT/SEGGER_RTT_printf.c | ✅ | RTT 格式化打印 |

---

## 十六、BSP 驱动 API 模板

以下是一个标准的 BSP 驱动文件应遵循的模式。注意：**没有 FreeRTOS 头文件，没有任务函数，没有 g_shm 引用。**

```c
// ============================================================
// BSP/RTC/ds3231.h — 示例: 标准的 BSP 驱动头文件
// ============================================================
#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "i2c.h"          // ← HAL I2C handle (hi2c1), NOT FreeRTOS

// BSP 驱动只导出功能函数:
// - 返回 HAL_StatusTypeDef 或 uint8_t (HAL_OK/HAL_ERROR)
// - 不返回 FreeRTOS 类型 (TickType_t, BaseType_t 等)
// - 参数不包含 g_shm 引用、osMessageQueueId_t 等

uint8_t ds3231_init(void);
uint8_t ds3231_read_time(uint8_t *hour, uint8_t *min, uint8_t *sec,
                          uint8_t *day, uint8_t *month, uint16_t *year);
void    ds3231_sleep(void);
void    ds3231_wakeup(void);

// ============================================================
// 反例 — BSP 文件绝对不能包含的内容:
// ============================================================
// ❌ #include "cmsis_os2.h"         ← BSP 不依赖 RTOS
// ❌ #include "FreeRTOS.h"          ← BSP 不依赖 RTOS
// ❌ #include "queue.h"             ← BSP 不使用队列
// ❌ #include "shared_memory.h"     ← BSP 不知道 g_shm
// ❌ #include "heartbeat.h"         ← BSP 不知道心跳
// ❌ xTaskCreate(...)               ← BSP 没有任务函数
// ❌ osDelay(...)                ← BSP 不调用调度器 API
// ❌ osMessageQueuePut(...)                ← BSP 不发送消息
// ❌ heartbeat_register(...)        ← 任务层才注册心跳
// ❌ g_shm.xxx = ...                ← BSP 不写共享内存
// ❌ osThreadFlagsSet(...)               ← BSP 不通知任务

#endif /* DS3231_H */
```

---

> 本文档是 XiYangWatch Phase 2 的**唯一权威参考**。  
> v4.1 修正了 v4.0 的任务分层偏差，使其符合 Zephyr/STM32Cube/NXP/Nordic/ARM 等行业标准。
> 代码怎么写, 怎么接, 怎么保护, 怎么写测试, 全在这里。  
> 填空题模式: 每个 Step 的文件名和核心代码骨架已给出, 往里面填即可。
