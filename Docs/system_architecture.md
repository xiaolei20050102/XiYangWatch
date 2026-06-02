# XiYangWatch 系统架构设计书

> 版本: v4.0  
> 日期: 2026-06-02  
> MCU: STM32F411CE (Cortex-M4F, 96MHz, 512KB Flash, 128KB RAM)  
> GUI: LVGL 9.5 (240×280, RGB565)  
> RTOS: FreeRTOS V10.3.1 (CMSIS-RTOS v2 API)

---

## 目录

1. [设计哲学](#一设计哲学)
2. [总线与硬件分配](#二总线与硬件分配)
3. [任务全景](#三任务全景)
4. [IPC 通信矩阵](#四ipc-通信矩阵)
5. [共享内存设计](#五共享内存设计)
6. [消息队列设计](#六消息队列设计)
7. [低功耗状态机](#七低功耗状态机)
8. [启动序列](#八启动序列)
9. [故障恢复策略](#九故障恢复策略)
10. [看门狗与心跳监控](#十看门狗与心跳监控)
11. [文件目录结构](#十一文件目录结构)
12. [编码规范](#十二编码规范)
13. [Phase 2 实施路线图](#十三phase-2-实施路线图)

---

## 一、设计哲学

### 1.1 核心原则

| 原则 | 含义 | 落地方式 |
|------|------|---------|
| **物理隔离** | 每个任务独占一条物理总线 | SPI1→LvglTask, I2C1→I2CSensTask, USART1→BtTask, SPI2→SaveTask |
| **零拷贝高频** | 高频数据不经过队列，走共享内存 | g_touch / g_sensor 直接读写，TaskNotify 通知 |
| **不可丢失低频** | 低频事件走消息队列，保证送达 | BLE 消息 / 存储请求 / 电源事件 走 Queue |
| **故障隔离** | 一个模块挂不影响系统运行 | 传感器降级、任务心跳超限软复位 |
| **可观测性** | 运行时状态透明，故障可追溯 | DiagTask 心跳监控 + UART2 日志输出 |
| **确定性** | 所有任务周期和优先级预先设计，不靠"运气"调度 | 高优先级抢占，低优先级填空 |

### 1.2 禁止事项

| 禁止 | 原因 |
|------|------|
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

## 二、总线与硬件分配

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

## 三、任务全景

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
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └──────┬───────────────┘ │
│       │            │            │              │                  │
│  ┌────┴────────────┴────────────┴──────────────┴───────────────┐ │
│  │  PowerTask (prio:10, 1KB, 1s)                                 │ │
│  │  电池/充电/亮暗屏/休眠仲裁                                     │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  DiagTask (prio:2, 1KB, 1s)                                   │ │
│  │  看门狗/心跳监控/异常复位/内存统计/UART2 日志                   │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  Tmr SvcTask (FreeRTOS 内建, 系统优先级)                       │ │
│  │  软件定时器回调执行                                             │ │
│  └──────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### 任务详细规格

| 属性 | LvglTask | I2CSensTask | BtTask | SaveTask | PowerTask | DiagTask |
|------|----------|-------------|--------|----------|-----------|----------|
| **优先级** | osPriorityHigh (12) | osPriorityHigh (12) | osPriorityNormal (10) | osPriorityLow (6) | osPriorityNormal (10) | osPriorityLow (2) |
| **栈大小** | 8KB (512×16) | 2KB (128×16) | 4KB (256×16) | 2KB (128×16) | 1KB (64×16) | 1KB (64×16) |
| **周期** | 5ms | 5ms | 事件驱动 | 队列驱动 | 1s | 1s |
| **独占硬件** | SPI1 | I2C1 | USART1 | SPI2 | ADC+GPIO | UART2 |
| **核心职责** | LVGL渲染+PageMgr | 5传感器采集 | BLE协议栈 | Flash持久化 | 电源管理 | 系统监控 |
| **心跳超时** | 1s | 200ms | 5s | 30s | 3s | — |

### 为什么 LvglTask 和 I2CSensTask 同优先级

两者都 5ms 周期，优先级相同。FreeRTOS 时间片轮转会交替执行：
- I2CSensTask 读完触摸写入 `g_touch`，下一个时间片 LvglTask 消费
- 同优先级保证了谁都不会抢占对方导致实时性抖动
- 如果 I2CSensTask 设更高优先级，它会在 LVGL 渲染中途打断导致帧撕裂

---

## 四、IPC 通信矩阵

### 4.1 完整通信图

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

### 4.2 每条通道的代码定义

```c
// ============================================================
// ipc_defs.h — 所有 IPC 对象统一定义于此
// ============================================================

// ——— Task Notifications (高频, 零拷贝) ———
// 只用作二进制通知(无值传递), 值走共享内存

// ——— Message Queues (低频, 不可丢失) ———
extern QueueHandle_t q_ble_msg;        // BtTask → LvglTask, BLE消息
extern QueueHandle_t q_save_req;       // I2CSensTask/BtTask → SaveTask, 存储请求
extern QueueHandle_t q_power_event;    // PowerTask → LvglTask, 电源事件
extern QueueHandle_t q_diag_log;       // 任意任务 → DiagTask, 诊断日志

// ——— Event Groups (多任务同步) ———
extern EventGroupHandle_t eg_sleep;    // PowerTask → 全员, 休眠协调
//   bit0: EVT_PREPARE_SLEEP   休眠准备
//   bit1: ACK_LVGL_READY      LvglTask 就绪
//   bit2: ACK_I2C_READY       I2CSensTask 就绪
//   bit3: ACK_BT_READY        BtTask 就绪
//   bit4: ACK_SAVE_READY      SaveTask 就绪
//   bit5: ACK_DIAG_READY      DiagTask 就绪
//   bit6: EVT_WAKEUP          系统唤醒

extern EventGroupHandle_t eg_sys_state; // 系统状态标志
//   bit0: SYS_READY           所有任务就绪
//   bit1: SYS_OTA_MODE        OTA 模式
//   bit2: SYS_SAFE_MODE       安全模式
```

### 4.3 为什么高频不走队列

```
触摸坐标: 每 5ms 一个 (200Hz)
  走队列: 每次 xQueueSend + xQueueReceive, 拷贝 12 字节, FreeRTOS 内部临界区
          → 200 次/秒 × 12 字节 × 2 = 4.8KB/秒 无效拷贝

  走共享内存: I2CSensTask 写 12 字节到固定地址
             LvglTask 读 12 字节
             → 零拷贝, 零延迟, 零临界区
             
  配合 TaskNotify: 一个无值的通知告知"新数据到了"
           32-bit 原子操作, 比信号量快 10 倍
```

---

## 五、共享内存设计

### 5.1 数据结构

```c
// ============================================================
// shared_memory.h — 所有全局共享数据统一定义于此
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
    uint32_t last_update_tick;  // 上次从 DS3231 读取的时间戳
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

### 5.2 访问规则

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
    xTaskNotify(lvgl_task_handle, 0, eNoAction);

读取示例 (LvglTask 中):
    touch_data_t t;
    t.x = g_shm.touch.x;  // 直接读, 无锁
    t.y = g_shm.touch.y;
```

---

## 六、消息队列设计

### 6.1 队列定义

```c
// ============================================================
// 消息类型枚举
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

// 队列: q_ble_msg, 深度 4, 元素大小 = sizeof(ble_msg_t)

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

// 队列: q_save_req, 深度 4, 元素大小 = sizeof(save_req_t)

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

// 队列: q_power_event, 深度 4, 元素大小 = sizeof(power_evt_t)
```

### 6.2 队列使用模式

```c
// 生产者 (例如 BtTask 发 BLE 通知消息)
ble_msg_t msg;
msg.type = BLE_MSG_NOTIFICATION;
msg.payload_len = len;
memcpy(msg.payload, data, len);

BaseType_t rc = xQueueSend(q_ble_msg, &msg, pdMS_TO_TICKS(100));
if (rc != pdPASS) {
    // 队列满 → 丢弃最老的消息, 插入新的
    ble_msg_t old;
    xQueueReceive(q_ble_msg, &old, 0);  // 非阻塞弹出旧消息
    xQueueSend(q_ble_msg, &msg, 0);     // 插入新消息
}

// 消费者 (LvglTask 收 BLE 消息)
ble_msg_t msg;
while (xQueueReceive(q_ble_msg, &msg, 0) == pdPASS) {
    // 一次处理完队列中所有消息, 避免积压
    lvgl_handle_ble_msg(&msg);
}
```

---

## 七、低功耗状态机

### 7.1 状态定义

```c
typedef enum {
    PWR_STATE_ACTIVE = 0,   // 全速: LCD 亮, 全传感器, BLE 活跃
    PWR_STATE_IDLE,         // 暗屏: LCD 最低亮, 传感器降频, BLE 正常
    PWR_STATE_STOP,         // 休眠: CPU STOP, 仅 RTC+触摸中断活着
    _PWR_STATE_COUNT
} power_state_t;
```

### 7.2 状态转换

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

### 7.3 PowerTask 核心逻辑

```c
void PowerTask(void *pvParameters)
{
    power_state_t state = PWR_STATE_ACTIVE;
    uint32_t last_user_event = HAL_GetTick();

    while (1)
    {
        // ① 读电池
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
            enter_stop_mode();  // 见 7.4
            last_user_event = HAL_GetTick();  // 醒来后重置计时
            state = PWR_STATE_ACTIVE;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 7.4 STOP 模式进入/退出

```c
static void enter_stop_mode(void)
{
    // ① 广播休眠准备
    xEventGroupSetBits(eg_sleep, EVT_PREPARE_SLEEP);

    // ② 等待所有任务就绪 (超时 2s, 强制继续)
    EventBits_t ack = xEventGroupWaitBits(
        eg_sleep,
        ACK_LVGL_READY | ACK_I2C_READY | ACK_BT_READY |
        ACK_SAVE_READY | ACK_DIAG_READY,
        pdTRUE,   // 读后清零
        pdTRUE,   // 等待所有位
        pdMS_TO_TICKS(2000)
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
    xEventGroupSetBits(eg_sleep, EVT_WAKEUP);

    // ⑤ 恢复外设
    HAL_SPI_Init(&hspi1);
    lcd_init_light();
    cst816_wakeup();
}
```

### 7.5 传感器降频表

| 传感器 | ACTIVE | IDLE | STOP |
|--------|:------:|:----:|:----:|
| CST816 (触摸) | 5ms | 20ms | RTC 500ms 微检 |
| QMI8658 (IMU) | 20ms | 100ms | 停 |
| MAX30102 (心率) | 100ms | 停 | 停 |
| DS3231 (RTC) | 1s | 1s | 停 |
| BME280 (环境) | 1s | 停 | 停 |

---

## 八、启动序列

```
上电复位
  │
  ▼
Phase 0: main() — 硬件初始化 (main.c, 跑在特权模式)
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
  │     ├── q_ble_msg         = xQueueCreate(4, sizeof(ble_msg_t))
  │     ├── q_save_req        = xQueueCreate(4, sizeof(save_req_t))
  │     ├── q_power_event     = xQueueCreate(4, sizeof(power_evt_t))
  │     ├── eg_sleep          = xEventGroupCreate()
  │     └── eg_sys_state      = xEventGroupCreate()
  ├── 创建 6 任务 (全部挂起: osThreadNew 默认挂起或创建后 vTaskSuspend)
  │     ├── LvglTask
  │     ├── I2CSensTask
  │     ├── BtTask  (Phase 3)
  │     ├── SaveTask (Phase 4)
  │     ├── PowerTask
  │     └── DiagTask
  └── osKernelStart()
         │
         ▼
Phase 2: 每个任务的 Self-Test
  各任务自行初始化, 完成后向 DiagTask 注册心跳:
  
  LvglTask:
    ├── lv_init() → lv_port_disp_init() → lv_port_indev_init()
    ├── w25q64_fs_init() (如需要)
    ├── app_init() → PageManager 创建首页
    └── heartbeat_register(TASK_LVGL, 1000)
  
  I2CSensTask:
    ├── CST816_Init()
    ├── I2C 总线扫描 0x01~0x7F → 记录已连接设备
    ├── 逐个初始化在线传感器 (DS3231/BME280/QMI8658/MAX30102)
    ├── 标记离线传感器为 SENS_FAILED
    └── heartbeat_register(TASK_I2CSENS, 200)
  
  PowerTask:
    ├── ADC 校准
    ├── 读初始电池电压
    └── heartbeat_register(TASK_POWER, 3000)
  
  DiagTask:
    ├── 读 RCC_GetResetFlags() → 记录复位原因
    ├── UART2 输出系统信息
    └── 等待所有任务首轮心跳
         │
         ▼
Phase 3: Ready → Run
  ├── DiagTask: 所有任务首轮心跳 OK → eg_sys_state |= SYS_READY
  └── 正常运行
```

---

## 九、故障恢复策略

### 9.1 故障分级

| 级别 | 现象 | 恢复策略 |
|:--:|------|---------|
| **L1** | 单个传感器通信失败 | 标记 SENS_FAILED, UI 显示"--", 其他传感器正常工作 |
| **L2** | 传感器连续重试 3 次仍失败 | 标记 SENS_FAILED, 5 分钟后尝试恢复 |
| **L3** | I2C 总线完全不通(4 个以上传感器 FAILED) | I2C1 复位 + 重新初始化, 仍不通 → 软复位 |
| **L4** | 任务心跳超时 | DiagTask 记录 → 软复位 |
| **L5** | 软复位 3 次内重演 | 进入 Safe Mode (仅 表盘+触摸) |
| **L6** | IWDG 复位 | 记录原因 → 正常启动 |

### 9.2 传感器接口抽象

```c
// ============================================================
// sens_driver.h — 传感器驱动统一接口
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

    /* 虚函数表 — 每个传感器填自己的实现 */
    sens_init_result_t (*init)(void);
    uint8_t            (*read)(void);       // 返回 HAL_OK 或 HAL_ERROR
    void               (*sleep)(void);
    void               (*wakeup)(void);
} sens_driver_t;

// 宏: 驱动注册
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

### 9.3 传感器读取包装 (带故障恢复)

```c
// I2CSensTask 内部
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

    uint8_t rc = drv->read();

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

## 十、看门狗与心跳监控

### 10.1 双层看门狗

```
Layer 1 — IWDG (硬件)
  ├── 时钟: LSI 32kHz, CPU 停了照样跑
  ├── 超时: 2s
  ├── 喂狗: DiagTask 每 1s 喂一次
  └── CPU 真的挂死 → 2s 后硬件复位

Layer 2 — 软件心跳 (应用层)
  ├── 每个任务注册心跳超时值
  ├── DiagTask 每 1s 扫描所有任务心跳
  ├── 超时 → UART2 打印 "DEAD: TaskName (last: Xms)"
  └── → 软复位 (NVIC_SystemReset)
```

### 10.2 心跳 API

```c
// ============================================================
// heartbeat.h — 任务心跳监控
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

### 10.3 实现

```c
// heartbeat.c

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
            diag_printf("[DEAD] Task %d timeout (%lu ms, #%lu)\r\n",
                        i, g_heartbeats[i].timeout_ms, g_heartbeats[i].dead_count);
            
            if (g_heartbeats[i].dead_count >= 3) {
                diag_printf("[FATAL] Soft reset\r\n");
                HAL_Delay(50);  // 等 UART 发完
                NVIC_SystemReset();
            }
        } else {
            g_heartbeats[i].dead_count = 0;  // 恢复清零
        }
    }
}
```

---

## 十一、文件目录结构

```
XiYang_Watch/
│
├── Core/
│   ├── Inc/
│   │   └── FreeRTOSConfig.h        ← configUSE_TICKLESS_IDLE=1
│   └── Src/
│       ├── main.c                   ← Phase 0 硬件初始化
│       ├── freertos.c               ← Phase 1 任务+IPC创建
│       └── stm32f4xx_it.c           ← 中断服务
│
├── App/
│   ├── app.c / app.h                ← app_init / app_loop
│   │
│   ├── Framework/
│   │   ├── ipc_defs.h              ← 所有 Queue/EventGroup 声明
│   │   ├── ipc_defs.c              ← IPC 对象定义 (全局句柄)
│   │   ├── heartbeat.h             ← 心跳注册宏/API
│   │   ├── heartbeat.c             ← 心跳监控实现
│   │   ├── shared_memory.h         ← g_shm 结构体定义
│   │   ├── shared_memory.c         ← g_shm 实例定义
│   │   ├── sens_driver.h           ← 传感器驱动抽象接口
│   │   ├── error_handler.h         ← 错误码 + 故障记录
│   │   ├── error_handler.c
│   │   ├── page_manager.h          ← (已有)
│   │   ├── page_manager.c          ← (已有)
│   │   ├── gesture.h               ← (已有)
│   │   └── gesture.c               ← (已有)
│   │
│   ├── Data/
│   │   ├── data_provider.h         ← (已有, 改指向 g_shm)
│   │   └── data_provider.c         ← (已有, 去除 USE_FAKE_DATA)
│   │
│   └── Pages/                       ← (已有, 保持不变)
│       ├── page_watchface.c
│       ├── page_heartrate.c
│       ├── page_control_center.c
│       ├── page_menu.c
│       ├── ...
│       └── pages_config.h
│
├── BSP/
│   ├── I2CSens/
│   │   ├── i2c_sens_task.h         ← I2CSensTask 入口
│   │   ├── i2c_sens_task.c         ← 主循环 + 传感器调度表
│   │   └── i2c_bus_scan.c          ← I2C 总线扫描工具
│   │
│   ├── Touch/
│   │   ├── touch_cst816s.h         ← (已有, 保持)
│   │   └── touch_cst816s.c         ← (已有, 保持)
│   │
│   ├── IMU/
│   │   ├── qmi8658.h
│   │   └── qmi8658.c               ← 新驱动 (Phase 2)
│   │
│   ├── HR/
│   │   ├── max30102.h
│   │   └── max30102.c              ← 新驱动 (Phase 2)
│   │
│   ├── ENV/
│   │   ├── bme280.h
│   │   └── bme280.c                ← 新驱动 (Phase 2)
│   │
│   ├── RTC/
│   │   ├── ds3231.h
│   │   └── ds3231.c                ← 新驱动 (Phase 2)
│   │
│   ├── LCD/
│   │   ├── lcd_st7789.h            ← (已有)
│   │   └── lcd_st7789.c            ← (已有)
│   │
│   ├── Flash/
│   │   ├── w25q64.h                ← (已有)
│   │   ├── w25q64.c                ← (已有)
│   │   ├── w25q64_port.h
│   │   └── w25q64_port.c
│   │
│   └── BLE/
│       ├── esp32_at.h              ← (Phase 3)
│       └── esp32_at.c
│
├── Middlewares/
│   └── LVGL/
│       ├── porting/
│       │   ├── lv_port_disp.c      ← (已有)
│       │   └── lv_port_indev.c     ← 改: 从 g_shm.touch 读, 不直接调 CST816
│       └── lv_conf.h
│
└── Docs/
    ├── system_architecture.md      ← 本文档
    ├── architecture.md             ← (已有, 旧版)
    ├── learning/
    │   ├── 01_ST7789_驱动原理与SPI通信详解.md
    │   ├── 02_CST816S_驱动原理与I2C通信详解.md
    │   └── 03_I2C通信协议详解.md
    └── pin_assignment.md
```

---

## 十二、编码规范

### 12.1 命名规则

```c
// 全局变量: g_ 前缀
volatile shared_mem_t g_shm;
QueueHandle_t         g_q_ble_msg;  // 旧代码兼容, 新 IPC 全用 Framework 里定义

// 静态变量: s_ 前缀
static uint32_t s_last_tick;
static sens_driver_t s_driver_ds3231;

// 常量: k_ 前缀
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

### 12.2 HAL 返回值检查

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

### 12.3 任务函数标准模板

```c
void ExampleTask(void *pvParameters)
{
    // ═══ 阶段 1: 初始化 ═══
    // 硬件初始化, 分配资源 (不允许 malloc, 用静态或栈分配)
    
    // ═══ 阶段 2: 自检 ═══
    // 验证硬件是否在线, 通信是否正常
    
    // ═══ 阶段 3: 注册心跳 ═══
    heartbeat_register(TASK_EXAMPLE, 1000);  // 1s 超时
    
    // ═══ 阶段 4: 主循环 ═══
    while (1)
    {
        // 4a. 接收 IPC 消息 (Queue / Notify / EventGroup)
        // 4b. 处理业务逻辑
        // 4c. 发送 IPC 消息
        // 4d. 更新心跳
        heartbeat_tick(TASK_EXAMPLE);
        
        // 4e. 阻塞等待 (vTaskDelay / 队列阻塞 / 信号量)
        //     ↑ 必须有这一步, 不能空转! ↑
    }
}
```

### 12.4 禁止模式清单

```c
// ❌ 禁止 1: 忙等
while (SPI_IS_BUSY()) {}  // CPU 100%

// ❌ 禁止 2: 驱动层调应用层
void cst816_read(void) {
    lvgl_update_ui();  // 层次颠倒!
}

// ❌ 禁止 3: 跨任务直接调函数
// LvglTask 中:
bt_send_message("hello");  // 直接调 BtTask 的函数!

// ❌ 禁止 4: HAL_Delay 在 FreeRTOS 任务中 (阻塞调度器)
HAL_Delay(100);  // 应该用 vTaskDelay(pdMS_TO_TICKS(100))

// ❌ 禁止 5: 运行时 malloc/free
uint8_t *buf = malloc(256);  // 碎片!
// 应该用栈变量或静态缓冲区
```

---

## 十三、Phase 2 实施路线图

> **当作填空题, 按步骤做, 每步做完可独立验证。**

### Step 0: 环境检查

- [ ] 确认 `FreeRTOSConfig.h` 关键配置:
  ```c
  #define configUSE_TICKLESS_IDLE          1
  #define configCHECK_FOR_STACK_OVERFLOW   2
  #define configUSE_TASK_NOTIFICATIONS     1
  #define configTICK_RATE_HZ               1000
  ```
- [ ] 确认 UART2 可用 (PA2 TX, 115200)
- [ ] 确认 I2C1 工作正常 (用 CST816_Test 验证)

### Step 1: IPC 基础设施 (预计 2h)

创建文件:
- `App/Framework/ipc_defs.h` + `ipc_defs.c`
- `App/Framework/shared_memory.h` + `shared_memory.c`
- `App/Framework/heartbeat.h` + `heartbeat.c`

创建全局对象:
```c
// ipc_defs.c
QueueHandle_t    g_q_ble_msg;
QueueHandle_t    g_q_save_req;
QueueHandle_t    g_q_power_event;
EventGroupHandle_t g_eg_sleep;
EventGroupHandle_t g_eg_sys_state;

// shared_memory.c
volatile shared_mem_t g_shm = {0};
```

验证: 编译通过, 无链接错误

### Step 2: DiagTask (预计 1h)

```c
// 新文件: App/Framework/diag_task.c
void DiagTask(void *pvParameters)
{
    // 读复位原因
    uint32_t rst_flag = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRSTF);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    
    diag_printf("\r\n=== XiYangWatch v1.0 ===\r\n");
    diag_printf("Reset: %s\r\n", rst_flag ? "IWDG" : "POR");
    
    // 注册心跳 (自己监控自己)
    heartbeat_register(TASK_DIAG, 3000);
    
    while (1) {
        heartbeat_monitor_all();  // 扫描所有任务
        
        // 内存统计
        diag_printf("[DIAG] FreeHeap:%u LVGL:StackHWM:%u\r\n",
                    xPortGetFreeHeapSize(),
                    uxTaskGetStackHighWaterMark(NULL));  // NULL=自己
        
        heartbeat_tick(TASK_DIAG);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

验证: 启动后 UART2 看到启动横幅 + 内存统计

### Step 3: PowerTask (预计 1.5h)

```c
// 新文件: App/Framework/power_task.c
void PowerTask(void *pvParameters)
{
    heartbeat_register(TASK_POWER, 3000);
    
    while (1) {
        // TODO: ADC 读电池
        g_shm.power.battery_pct = 85;  // 先写死, 后面接 ADC
        
        // TODO: 休眠状态机 (先只做 ACTIVE, STOP 后面加)
        
        heartbeat_tick(TASK_POWER);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

验证: 任务跑起来, 心跳正常

### Step 4: I2CSensTask — 触摸搬迁 (预计 2h)

这是 Phase 2 最关键的步骤:

```c
// 新文件: BSP/I2CSens/i2c_sens_task.c
#include "cmsis_os.h"
#include "touch_cst816s.h"
#include "shared_memory.h"
#include "heartbeat.h"

// LvglTask 的句柄 (外部声明, 用于 TaskNotify)
extern TaskHandle_t g_lvgl_task_handle;

void I2CSensTask(void *pvParameters)
{
    // ① 触摸初始化
    CST816_Init();
    
    // ② I2C 总线扫描 (看看还有哪些设备在线)
    // i2c_bus_scan();  ← 先不写, 后面补
    
    // ③ 注册心跳
    heartbeat_register(TASK_I2CSENS, 200);
    
    uint8_t counter = 0;
    
    while (1)
    {
        // ④ 读触摸 (每周期)
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
        xTaskNotify(g_lvgl_task_handle, 0, eNoAction);  // 通知 LVGL
        
        // ⑤ 其他传感器 (按频率分档, 后面补)
        // [每200次] DS3231
        // [每200次] BME280
        // [每4次]   QMI8658
        // [每10次]  MAX30102
        
        // ⑥ 传感器健康状态
        g_shm.sens_health.touch = SENS_OK;
        
        // ⑦ 心跳 + 阻塞
        heartbeat_tick(TASK_I2CSENS);
        counter++;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

**同时修改 `lv_port_indev.c`**: 改为从 `g_shm.touch` 读, 不再直接调 CST816:

```c
// lv_port_indev.c
#include "shared_memory.h"

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

**修改 `freertos.c`**: 创建 I2CSensTask + DiagTask + PowerTask:

```c
// freertos.c
#include "shared_memory.h"
#include "ipc_defs.h"
#include "heartbeat.h"

TaskHandle_t g_lvgl_task_handle;  // 供 I2CSensTask 用

void StartLvglTask(void *argument)
{
    g_lvgl_task_handle = xTaskGetCurrentTaskHandle();
    
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();  // 内部不再调 CST816_Init
    app_init();
    
    heartbeat_register(TASK_LVGL, 1000);
    
    for (;;) {
        uint8_t raw = CST816_GetGesture();  // ← TODO: 之后也搬走
        // ... gesture_feed ...
        
        lv_timer_handler();
        app_loop();
        
        heartbeat_tick(TASK_LVGL);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

验证: **触摸功能完全正常** — 这是最关键的 checkpoint。如果触摸不行, 回退排查, 不要继续。

### Step 5: DS3231 驱动 (预计 1.5h)

```
BSP/RTC/ds3231.h + ds3231.c

和 CST816 套路完全一样:
  ① I2C 地址 0x68
  ② 读 7 个时间寄存器 (秒/分/时/星期/日/月/年)
  ③ 在 I2CSensTask 里每 200 个周期读一次
  ④ 写 g_shm.time
  ⑤ 表盘不再读假时间
```

验证: 表盘显示真实时间

### Step 6: BME280 驱动 (预计 2h)

```
BSP/ENV/bme280.h + bme280.c

  ① I2C 地址 0x76
  ② 初始化: 配置过采样率 / IIR滤波器 / 模式
  ③ 读温度/湿度/气压补偿寄存器
  ④ 套公式算出真实值
  ⑤ 写 g_shm.env
```

验证: 环境页面显示真实温湿气压

### Step 7: QMI8658 驱动 (预计 2.5h)

```
BSP/IMU/qmi8658.h + qmi8658.c

  ① I2C 地址 0x6B
  ② 初始化: 配置加速度量程 / 陀螺仪量程 / ODR
  ③ 读加速度 XYZ + 陀螺仪 XYZ
  ④ I2CSensTask 内部做计步累计
  ⑤ 写 g_shm.imu
```

验证: 活动页面步数变化

### Step 8: MAX30102 驱动 (预计 3h)

```
BSP/HR/max30102.h + max30102.c

  ① I2C 地址 0x57
  ② 初始化: 配置 LED 电流 / FIFO / 采样率
  ③ 读 FIFO → 提取红光/红外数据
  ④ 心率算法 (查表或滑动窗口)
  ⑤ 写 g_shm.hr
```

验证: 心率页面显示真实心率

### Step 9: data_provider 去假数据 (预计 1h)

```c
// data_provider.c — 删除 USE_FAKE_DATA, 全部指向 g_shm
#include "shared_memory.h"

uint8_t watch_data_get_heart_rate(void) {
    return g_shm.hr.hr_valid ? g_shm.hr.hr_bpm : 0;
}
uint32_t watch_data_get_steps(void) {
    return g_shm.imu.steps;
}
// ... 等等
```

验证: 所有页面数据来自真实传感器

### Step 10: 停机整理

- [ ] 确认所有 HAL 调用检查返回值
- [ ] 确认所有任务 while(1) 有阻塞点
- [ ] 运行 30 分钟, 确认心跳无超时
- [ ] 运行 30 分钟, 确认无内存泄露 (xPortGetFreeHeapSize 稳定)
- [ ] 所有新代码加 doxygen 注释

---

> 本文档是 XiYangWatch Phase 2 的**唯一权威参考**。  
> 代码怎么写, 怎么接, 怎么保护, 怎么写测试, 全在这里。  
> 填空题模式: 每个 Step 的文件名和核心代码骨架已给出, 往里面填即可。
