# 智能手环软件架构设计方案

## Context

基于用户的硬件清单（STM32F411 + ESP32-C3 + 7个I2C传感器 + SPI LCD/Flash），参考 OV-Watch 2.4.3 项目架构，设计一套**面向学习**的嵌入式软件架构。

核心目标：
1. 从 OV-Watch 中提取经过验证的好模式（PageManager、HW抽象）
2. 修复 OV-Watch 的架构缺陷（13个任务混跑、无互斥、bit-bang I2C 等）
3. 引入规范的嵌入式设计模式（EventCenter、Manager 模式、状态机）
4. 分层清晰，每层不越界，便于分阶段学习和实现

---

## 一、总体架构分层

```
┌─────────────────────────────────────────┐
│  App/  应用层                            │
│  ├── Pages/      各UI页面               │
│  ├── app_page_manager.c  页面栈导航     │
│  └── app_tasks.c        任务创建&初始化  │
├─────────────────────────────────────────┤
│  System/  系统服务层                     │
│  ├── sys_event_center    事件中心        │
│  ├── sys_sensor_manager  传感器管理器    │
│  ├── sys_ble_manager     BLE管理器       │
│  ├── sys_power_manager   电源管理器      │
│  ├── sys_storage_manager 存储管理器      │
│  └── sys_watchdog        看门狗          │
├─────────────────────────────────────────┤
│  BSP/  板级驱动层                        │
│  ├── bsp_spi/i2c/uart/adc  总线抽象     │
│  ├── lcd_st7789            显示屏        │
│  ├── touch_cst816s         触摸          │
│  ├── imu_qmi8658           IMU          │
│  ├── hr_max30102           心率血氧      │
│  ├── env_bme280            环境传感器    │
│  ├── als_bh1750            环境光        │
│  ├── eeprom_at24c256       EEPROM       │
│  ├── flash_w25q128         SPI Flash    │
│  ├── rtc_ds3231            外部RTC       │
│  ├── power_battery         电池/充电     │
│  └── button_driver         按键          │
├─────────────────────────────────────────┤
│  Core/  STM32 HAL层                      │
├─────────────────────────────────────────┤
│  Drivers/  CMSIS + HAL 库               │
└─────────────────────────────────────────┘
```

## 二、目录结构

```
SmartBand/
├── BSP/                              # 板级支持包
│   ├── Inc/                          # BSP公共头文件
│   │   ├── bsp_pin_config.h          # ★ 所有引脚定义集中在一个文件
│   │   ├── bsp_spi.h
│   │   ├── bsp_i2c.h                 # 硬件I2C（非bit-bang）
│   │   ├── bsp_uart.h
│   │   └── bsp_adc.h
│   ├── Src/
│   │   ├── bsp_spi.c
│   │   ├── bsp_i2c.c
│   │   ├── bsp_uart.c
│   │   └── bsp_adc.c
│   ├── LCD/          (lcd_st7789.h/.c)
│   ├── Touch/        (touch_cst816s.h/.c)
│   ├── IMU/          (imu_qmi8658.h/.c)
│   ├── HeartRate/    (hr_max30102.h/.c)
│   ├── Environmental/(env_bme280.h/.c)
│   ├── AmbientLight/ (als_bh1750.h/.c)
│   ├── EEPROM/       (eeprom_at24c256.h/.c)
│   ├── Flash/        (flash_w25q128.h/.c)
│   ├── RTC/          (rtc_ds3231.h/.c)
│   ├── Power/        (power_battery.h/.c)
│   ├── Vibration/    (vibe_motor.h/.c)
│   └── Button/       (button_driver.h/.c)
│
├── Core/                             # STM32 HAL初始化
│   ├── Inc/
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   ├── stm32f4xx_it.h
│   │   └── FreeRTOSConfig.h
│   ├── Src/
│   │   ├── main.c                    # 入口：HAL_Init -> SystemClock -> osKernelStart
│   │   ├── stm32f4xx_hal_msp.c       # MSP初始化
│   │   ├── stm32f4xx_it.c            # 中断服务函数
│   │   └── freertos.c                # MX_FREERTOS_Init
│   └── Startup/
│       └── startup_stm32f411xe.s
│
├── Drivers/                          # CMSIS + HAL（不动）
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
│
├── Middlewares/                      # 第三方库
│   ├── FreeRTOS/
│   └── LVGL/
│       ├── lv_conf.h
│       └── porting/
│           ├── lv_port_disp.c/.h
│           ├── lv_port_indev.c/.h
│           └── lv_port_fs.c/.h       # SPI Flash文件系统
│
├── System/                           # ★ 系统服务层（核心创新）
│   ├── Inc/
│   │   ├── sys_common.h              # 项目级类型/错误码/配置
│   │   ├── sys_os.h                  # OS抽象
│   │   ├── sys_event_center.h        # 事件中心
│   │   ├── sys_sensor_manager.h      # 传感器管理器
│   │   ├── sys_ble_manager.h         # BLE管理器
│   │   ├── sys_power_manager.h       # 电源管理器
│   │   ├── sys_storage_manager.h     # 存储管理器
│   │   └── sys_watchdog.h            # 看门狗
│   └── Src/
│       ├── sys_event_center.c
│       ├── sys_sensor_manager.c
│       ├── sys_ble_manager.c
│       ├── sys_power_manager.c
│       ├── sys_storage_manager.c
│       └── sys_watchdog.c
│
├── App/                              # 应用层
│   ├── Inc/
│   │   ├── app_page_manager.h        # 页面栈导航
│   │   ├── app_pages.h               # 页面注册表
│   │   └── app_tasks.h               # 任务声明
│   ├── Src/
│   │   ├── app_page_manager.c
│   │   ├── app_tasks.c               # ★ 任务创建（6个任务）
│   │   └── app_startup.c             # 启动流程编排
│   ├── Pages/
│   │   ├── Home/     (page_home.h/.c)
│   │   ├── Menu/     (page_menu.h/.c)
│   │   ├── HeartRate/(page_heartrate.h/.c)
│   │   ├── SpO2/     (page_spo2.h/.c)
│   │   ├── Environment/(page_env.h/.c)
│   │   ├── Settings/ (page_settings.h/.c)
│   │   └── Charging/ (page_charging.h/.c)
│   └── Resources/
│       ├── Fonts/
│       └── Images/
│
├── Sim/                              # PC仿真层
│   ├── sim_hal.h/.c
│   ├── sim_bsp.h/.c
│   └── main_sim.c
│
└── Tests/                            # 单元测试
    ├── test_event_center.c
    ├── test_sensor_manager.c
    └── test_page_manager.c
```

## 三、6个FreeRTOS任务（精简自OV-Watch的13个）

| # | 任务名 | 优先级 | 栈(byte) | 周期 | 职责 |
|---|--------|--------|----------|------|------|
| 1 | **EventCenterTask** | High(8) | 4096 | 事件驱动 | 从队列取出事件，分发给订阅者 |
| 2 | **BLETask** | Normal(5) | 8192 | 事件驱动 | UART与ESP32-C3通信 |
| 3 | **SensorTask** | Normal(4) | 8192 | 50ms | 按调度表轮询传感器，发布事件 |
| 4 | **PowerTask** | Normal(4) | 2048 | 100ms | 空闲检测、状态迁移、STOP进出 |
| 5 | **UITask** | Low(3) | 16384 | 5ms | `lv_timer_handler()`、页面动画 |
| 6 | **StorageTask** | Low(3) | 4096 | 事件驱动 | 延迟EEPROM/Flash写入 |

**优先级设计原理**：
- EventCenterTask 最高 —— 如果它被饿死，整个系统的通信中断
- BLETask 次高 —— UART DMA缓冲区小，必须及时消费
- Sensor/Power 中优先级 —— 定时轮询，允许偶尔抖动
- UI/Storage 最低 —— 丢帧可接受，写入可延迟

**与OV-Watch的对比**：OV-Watch有13个任务，其中6个是 osPriorityLow1 同优先级，时间片轮转导致不可预测的延迟。

## 四、核心设计——EventCenter事件中心

这是与OV-Watch最大的架构差异。OV-Watch的pubsub未实际使用；我们用事件总线替代所有直接调用。

### 事件类型（部分）

| 类别 | 事件 | 说明 |
|------|------|------|
| 传感器 | EVENT_SENSOR_HEART_RATE | 心率数据就绪 |
| 传感器 | EVENT_SENSOR_STEPS | 步数更新 |
| 传感器 | EVENT_SENSOR_WRIST_UP/DOWN | 翻腕检测 |
| 传感器 | EVENT_SENSOR_BATTERY | 电量更新 |
| 传感器 | EVENT_SENSOR_AMBIENT_LIGHT | 环境光照度 |
| 输入 | EVENT_INPUT_BUTTON_SHORT/LONG | 按键短按/长按 |
| 输入 | EVENT_INPUT_TOUCH_GESTURE | 触摸手势 |
| BLE | EVENT_BLE_DATA_RECEIVED | 手机发来数据 |
| BLE | EVENT_BLE_CONNECTED/DISCONNECTED | 连接状态变化 |
| 电源 | EVENT_POWER_CHARGING_START/STOP | 充电插拔 |
| 电源 | EVENT_POWER_BATTERY_LOW | 低电量警告 |

### 核心API

```c
// 初始化事件中心
bool EventCenter_Init(max_subscribers, max_queue_len);

// 订阅事件（返回句柄用于取消订阅）
uint32_t EventCenter_Subscribe(EventType_t type, EventCallback_t callback);

// 发布事件（线程安全，ISR版本可用于中断）
bool EventCenter_Publish(const Event_t* event);
bool EventCenter_PublishFromISR(const Event_t* event);
```

### 线程安全策略

- 发布者 → 写入 FreeRTOS 队列 → 不阻塞
- EventCenterTask → 从队列读取 → 依次调用订阅者回调
- 回调运行在 EventCenterTask 上下文，**不能直接操作LVGL对象**（LVGL非线程安全）
- 回调只设置标志/缓存数据；UITask 在 `on_update()` 中修改LVGL

### 数据流示例：翻腕亮屏

```
QMI8658 INT引脚 → ISR → EventCenter_PublishFromISR(WRIST_UP)
  → EventCenterTask 取出事件
    → PowerManager 订阅者: ExitStop() 唤醒屏幕
    → HomePage 订阅者: 设置 g_refresh_pending = true
      → UITask 下次循环: Page.on_update() → 刷新时间/步数
```

与OV-Watch的对比：OV-Watch是 MPUCheckTask 每300ms轮询 → 直接写全局变量 → StopEnterTask 轮询队列。

## 五、Manager模式设计要点

### SensorManager（传感器管理器）
- **唯一拥有者**：所有I2C传感器只通过SensorManager访问
- I2C总线互斥锁保护
- 采样模式：ONESHOT（按需）/ PERIODIC（定时）/ STREAM（连续）
- 数据缓存快照（mutex保护），UI层只读快照
- 自动发布事件

### PowerManager（电源管理器）
- 四级状态机：ACTIVE → IDLE → SLEEP → STOP
- 空闲定时器驱动迁移
- STOP模式进出统一管理（去掉了OV-Watch的 `goto sleep`）
- 唤醒源：Button INT / Touch INT / QMI8658 INT / RTC Alarm / Charge Detect

### BLEManager（BLE管理器）
- ESP32-C3通过UART1连接，使用帧协议：`[START][LEN][CMD][PAYLOAD][CRC16][END]`
- 连接状态机：OFF → INIT → ADVERTISING → CONNECTED
- 服务抽象：TimeSync / HeartRate / Battery / OTA

### StorageManager（存储管理器）
- EEPROM(AT24C256): 配置/BLE信息/校准数据，简单磨损均衡
- SPI Flash(W25Q128): 字体/图片/OTA缓存
- 延迟写入队列，避免I2C冲突

## 六、PageManager（继承自OV-Watch，增强）

```c
typedef struct {
    const char* name;
    void (*on_create)(lv_obj_t* parent);   // 页面入栈时
    void (*on_destroy)(void);              // 页面出栈时
    void (*on_update)(const void* data);   // 数据更新（UITask上下文）
    void (*on_event)(uint32_t event_id);   // 系统事件（可跨任务）
    lv_obj_t* screen;
} Page_t;
```

**比OV-Watch的改进**：
- 新增 `on_update` — UI层不再直接轮询全局变量
- 新增 `on_event` — 页面可响应系统事件（如超时锁屏）
- `name` 字段 — 可调试
- Stack操作有返回值 — 不会静默失败

## 七、关键架构决策对照

| 方面 | OV-Watch | 新架构 | 为什么更好 |
|------|----------|--------|------------|
| I2C实现 | GPIO bit-bang，CPU 100%忙等 | 硬件I2C + DMA | 学习DMA和中断驱动IO |
| 任务数 | 13个，优先级扁平 | 6个，优先级分层 | 可预测的调度行为 |
| 互斥 | `vTaskSuspendAll()` 挂起所有任务 | 细粒度mutex | 学习正确的临界区设计 |
| 模块通信 | 全局变量直读 | EventCenter事件总线 | 学习事件驱动架构 |
| 驱动模式 | 全局`HWInterface`结构体 | 不透明句柄 `Xxx_Create()` | 学习面向接口编程 |
| 电源管理 | `goto sleep` + 重复初始化代码 | 状态机 + 统一进出函数 | 可测试、可维护 |
| 引脚定义 | 分散在各驱动文件中 | 单文件 `bsp_pin_config.h` | 初学者友好 |
| UI数据流 | 页面定时器轮询全局变量 | 订阅事件 → 设置标志 → UITask刷新 | 线程安全 |

## 八、实现顺序（8个阶段）

### Phase 1: BSP驱动 + HAL初始化 + 点灯
- 构建项目骨架，配置CubeMX
- 实现 `bsp_pin_config.h`（所有引脚定义）
- 实现 `bsp_spi.c`、`bsp_i2c.c`、`bsp_uart.c`、`bsp_adc.c`
- 验证：I2C总线扫描、UART打印、LED闪烁

### Phase 2: FreeRTOS启动 + 基础任务
- 配置 FreeRTOSConfig.h
- 实现 `sys_os.h/.c`（OS抽象）
- 实现 `app_tasks.c`（创建2个测试任务）
- 实现 `sys_watchdog.c`（IWDG喂狗）

### Phase 3: 显示 + LVGL + 触摸
- 实现 `lcd_st7789.c`（硬件SPI + DMA）
- LVGL移植：`lv_port_disp.c`、`lv_port_indev.c`
- 实现 `touch_cst816s.c`
- 实现 `app_page_manager.c`（页面栈）

### Phase 4: 传感器 + SensorManager + EventCenter
- 实现 `sys_event_center.c`（事件总线）
- 实现各传感器驱动：QMI8658、BME280、BH1750、DS3231、power_battery
- 实现 `sys_sensor_manager.c`（统一调度）
- 创建 SensorTask

### Phase 5: 存储 + StorageManager
- 实现 `eeprom_at24c256.c`、`flash_w25q128.c`
- 实现 `sys_storage_manager.c`（键值存储 + 磨损均衡）
- 创建 StorageTask

### Phase 6: BLE + BLEManager
- ESP32-C3烧录UART桥接固件
- 实现帧协议解析器
- 实现 `sys_ble_manager.c`（状态机 + 服务抽象）
- 创建 BLETask

### Phase 7: 电源管理
- 实现 `sys_power_manager.c`（四级状态机）
- STOP模式进出（时钟门控、外设反初始化）
- 唤醒处理
- 创建 PowerTask

### Phase 8: 全部页面 + 系统联调
- 实现所有 UI 页面
- 对接 EventCenter 订阅
- 端到端测试：开机→导航→休眠→唤醒→BLE→OTA

---

## 验证方式

1. **单元测试**：EventCenter、SensorManager、PageManager、StorageManager 各模块用 PC 仿真验证
2. **集成测试**：每个 Phase 结束时在真实硬件上验证
3. **系统测试**：Phase 8 完成后完整跑通所有功能路径
4. **功耗测试**：Phase 7 完成后测量 Active/Idle/Sleep/STOP 各状态电流
