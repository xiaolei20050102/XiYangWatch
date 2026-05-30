# XiYangWatch 架构设计文档

> 版本: v3.1 (辐射式导航)  
> 日期: 2026-05-23  
> MCU: STM32F411CE (Cortex-M4F, 100MHz, 512KB Flash, 128KB RAM)  
> GUI: LVGL 9.5 (mono theme, 240×280)  
> RTOS: FreeRTOS V10.3.1 (CMSIS-RTOS v2)

---

## 设计原则

1. **分层解耦** — 每层只依赖下层，上层不知道底层硬件细节
2. **一次设计，渐进实现** — 18 个页面和全部接口一次性规划，按 Phase 分批落地
3. **低耦合接口预留** — 所有接口编译期定义，未实现的返回安全默认值
4. **可读性优先** — 目录命名自解释，文件命名见名知意
5. **稳中求进** — 每阶段只改一层，不改动 CubeMX 生成的代码

---

## 1. 硬件总览

### 1.1 模块清单 (11 个外设)

| 总线 | 芯片 | 地址/引脚 | 用途 | 状态 |
|------|------|-----------|------|------|
| SPI2 | ST7789 | PB13/PB14/PB15 | LCD 240×280 显示 | ✅ 完成 |
| I2C1 | CST816S | 0x15 | 电容触摸 | ✅ 完成 |
| I2C1 | QMI8658 | 0x6B | 6 轴 IMU (步数/姿态) | 🔲 Phase 2 |
| I2C1 | MAX30102 | 0x57 | 心率 + 血氧 | 🔲 Phase 2 |
| I2C1 | BME280 | 0x76 | 温度/湿度/气压 | 🔲 Phase 2 |
| I2C1 | BH1750 | 0x23 | 环境光 (自动亮度) | 🔲 Phase 2 |
| I2C1 | DS3231 | 0x68 | 外部 RTC | 🔲 Phase 2 |
| SPI1 | W25Q64 | PA4/PA5/PA6/PA7 | 8MB 外部 Flash (OTA) | 🔲 Phase 4 |
| I2C2 | AT24C256 | PB10/PB11 | EEPROM 存储 | 🔲 Phase 3 |
| UART1 | ESP32-C3 | PA9/PA10 | BLE 通信 | 🔲 Phase 3 |
| UART2 | Debug | PA2/PA3 | 调试串口 | ✅ 完成 |

### 1.2 传感器互斥关系

**MAX30102: HR 与 SpO2 模式互斥**

| 模式 | LED | 功耗 | 输出 |
|------|-----|------|------|
| HR 连续 | 红光 | 低 | 心率 bpm |
| SpO2 测量 | 红光+红外 | 高 | 心率 + 血氧% |

平时后台 HR 模式持续跑，用户点"测量血氧"时切 SpO2 模式 30 秒，完成后自动切回。

**BME280:** 一次测量同时出温度+湿度+气压，无模式冲突。

---

## 2. 目录架构

```
XiYang_Watch/
|
|-- Core/                          # CubeMX 生成层 —— 原则上不修改
|   |-- Inc/                        # HAL 外设头文件
|   |-- Src/                        # HAL 外设源文件 + main.c + freertos.c
|       |-- main.c                  # 硬件初始化 + 启动调度器
|       |-- freertos.c              # 任务创建 (仅创建，不含业务逻辑)
|
|-- Drivers/                        # CMSIS + HAL 库 —— 不修改
|
|-- BSP/                            # 板级驱动层 —— 每外设一个子目录
|   |-- LCD/lcd_st7789.c/h          # ST7789 显示驱动 ✅
|   |-- Touch/touch_cst816s.c/h     # CST816S 触摸驱动 ✅
|   |-- IMU/qmi8658.c/h             # QMI8658 6 轴 (Phase 2)
|   |-- HR/max30102.c/h             # MAX30102 心率血氧 (Phase 2)
|   |-- Env/bme280.c/h              # BME280 温湿度气压 (Phase 2)
|   |-- Light/bh1750.c/h            # BH1750 环境光 (Phase 2)
|   |-- RTC/ds3231.c/h              # DS3231 时钟 (Phase 2)
|   |-- Flash/w25q64.c/h            # W25Q64 外部 Flash (Phase 4)
|
|-- Middlewares/                     # 第三方中间件
|   |-- LVGL/
|   |   |-- src/                    # LVGL 9.5.0 源码
|   |   |-- porting/                # 移植适配层
|   |   |   |-- lv_port_disp.c      # 显示接口 (disp_flush → ST7789_FlushArea)
|   |   |   |-- lv_port_indev.c     # 触摸接口 (indev_read → CST816)
|   |   |-- fonts/                  # 自定义字体
|   |   |-- lv_conf.h               # LVGL 配置
|   |-- Third_Party/FreeRTOS/       # FreeRTOS 内核
|
|-- App/                            # ★ 应用层 —— 主要开发区域
|   |-- app.c/h                     # 入口: app_init() + app_loop()
|   |-- Pages/                      # View: LVGL UI 页面 (18 个)
|   |   |-- page_watchface.c        # Hub: 表盘
|   |   |-- page_heartrate.c        # Spoke: 心率 (←左)
|   |   |-- page_spo2.c             # Spoke: 血氧 (→右)
|   |   |-- page_control_center.c   # Spoke: 控制中心 (↑上)
|   |   |-- page_notifications.c    # Spoke: 通知 (↓下)
|   |   |-- page_menu.c             # Overlay: 菜单
|   |   |-- page_display.c          # Overlay: 显示与亮度
|   |   |-- page_watchface_sel.c    # Overlay: 表盘样式选择
|   |   |-- page_activity.c         # Overlay: 活动详情
|   |   |-- page_alarm.c            # Overlay: 闹钟
|   |   |-- page_stopwatch.c        # Overlay: 秒表
|   |   |-- page_timer.c            # Overlay: 计时器
|   |   |-- page_calculator.c       # Overlay: 计算器
|   |   |-- page_spo2_history.c     # Overlay: 血氧历史
|   |   |-- page_workout_history.c  # Overlay: 运动记录
|   |   |-- page_find_phone.c       # Overlay: 找手机
|   |   |-- page_bluetooth.c        # Overlay: 蓝牙
|   |   |-- page_about.c            # Overlay: 关于设备
|   |-- Framework/                  # Controller: 页面管理 + 手势
|   |   |-- page_manager.c/h        # Hub/Spoke/Overlay 三态管理
|   |   |-- gesture.c/h             # 手势分发
|   |-- Data/                       # Model: 数据提供
|       |-- data_provider.c/h       # 数据接口 (USE_FAKE_DATA 编译期切换)
|
|-- Bootloader/                     # OTA 引导加载器 (独立工程, Phase 4)
|   |-- bootloader.c                # CRC + Flash 烧写 + 跳转
|
|-- Docs/                           # 文档
|   |-- architecture.md             # 本文档
|   |-- pin_assignment.md           # 引脚分配表
|   |-- embedded-C-coding-standard.md
|   |-- learning/                   # 学习笔记
```

---

## 3. 分层架构

```
┌──────────────────────────────────────────┐
│  App/Pages/  (18 个页面)                  │  ← View
│  只依赖 Data/ 读数据、Framework/ 做切换   │
├──────────────────────────────────────────┤
│  App/Framework/  (page_manager, gesture)  │  ← Controller
├──────────────────────────────────────────┤
│  App/Data/  (data_provider)              │  ← Model
│  USE_FAKE_DATA 编译期切换                 │
├──────────────────────────────────────────┤
│  BSP/  (9 个外设驱动)                     │  ← Hardware Abstraction
├──────────────────────────────────────────┤
│  Middlewares/  (LVGL + FreeRTOS)         │
├──────────────────────────────────────────┤
│  Core/ + Drivers/  (HAL, CubeMX 生成)    │
└──────────────────────────────────────────┘
```

**依赖规则:**
- Pages → Data (只读数据)、Pages → Framework (页面切换)
- Data → BSP (读传感器)
- Framework → BSP (读触摸手势)
- 禁止: Pages 直接调 BSP、直接读写 I2C/SPI

---

## 4. 导航模型: 辐射式 (Hub-and-Spoke)

### 4.1 导航地图

```
                    ↑ 上滑
              ┌──────────┐
              │  控制中心  │  快捷开关 + 工具 + 菜单入口
              │  (上辐射)  │
              └────┬─────┘
                   │
    ← 左滑         │          右滑 →
┌──────────┐  ┌───┴───┐  ┌──────────┐
│   心率    │──│ 表 盘 │──│   血氧    │
│ (左辐射)  │  │ (Hub) │  │ (右辐射)  │
└──────────┘  └───┬───┘  └──────────┘
                   │
              ┌────┴─────┐
              │   通知    │  BLE 消息列表 (预留)
              │ (下辐射)  │
              └──────────┘
                   ↓ 下滑
```

### 4.2 辐射层: 5 个页面

| 方向 | 手势 | 页面 | 职责 |
|------|------|------|------|
| 中心 | — | 表盘 | 信息仪表盘 |
| ← 左 | CST816 GESTURE_LEFT | 心率 | 实时心率数字 + 区间 |
| → 右 | CST816 GESTURE_RIGHT | 血氧 | SpO2 测量 + 最近结果 + 跳历史 |
| ↑ 上 | CST816 GESTURE_UP | 控制中心 | 6 快捷图标 + 菜单入口 |
| ↓ 下 | CST816 GESTURE_DOWN | 通知 | BLE 消息列表 (Phase 3 接入) |

**规则:**
- 从任一辐射页任意方向滑 → 回到表盘（一键回家）
- 从表盘长按 1s → 表盘切换模式

### 4.3 压栈层 (Overlay): 13 个页面

从辐射页点击控件 → push 进入 → 左右滑 pop 返回。

| # | 页面 | 从哪里进入 | Phase |
|---|------|-----------|-------|
| 1 | 菜单 | 控制中心 ⚙ | P1 |
| 2 | 显示与亮度 | 控制中心 🔆 或菜单 | P1 |
| 3 | 表盘样式 | 菜单 (或长按表盘) | P1 |
| 4 | 活动详情 | 控制中心 👣 或菜单 | P1 |
| 5 | 闹钟列表 | 控制中心 ⏰ 或菜单 | P2 |
| 6 | 秒表 | 控制中心 ⏱ 或菜单 | P2 |
| 7 | 计时器 | 菜单 | P2 |
| 8 | 计算器 | 控制中心 🧮 或菜单 | P2 |
| 9 | 血氧历史 | 血氧页 或菜单 | P2 |
| 10 | 运动记录 | 菜单 | P2 |
| 11 | 找手机 | 菜单 | P4 |
| 12 | 蓝牙 | 菜单 | P3 |
| 13 | 关于设备 | 菜单 | P1 |

**路由复用:** 活动详情、闹钟、秒表、计算器、血氧历史 — 从控制中心图标和菜单列表都能进入，同一个 push/pop 终点，各回各家。

### 4.4 页面管理器三态

```c
// page_manager.h

typedef enum {
    STATE_AT_HUB,     // 在表盘
    STATE_AT_SPOKE,   // 在辐射页 (心率/血氧/控制中心/通知)
    STATE_AT_OVERLAY, // 在压栈页
} page_state_t;

void page_manager_init(void);
void page_manager_go_home(void);              // 回到表盘
void page_manager_go_spoke(int direction);    // 去辐射页 (LEFT/RIGHT/UP/DOWN)
void page_manager_push(int page_idx);         // 压入 overlay 页
void page_manager_pop(void);                  // 弹出 overlay 页
void page_manager_update(void);               // 刷新当前可见页
page_state_t page_manager_get_state(void);    // 当前状态
```

### 4.5 物理按键 (KEY1)

| 按键 | 引脚 | 短按 | 后续扩展 |
|------|------|------|---------|
| KEY1 | PA0 | 返回: Overlay→pop / Spoke→回表盘 / Hub→熄屏 | 长按关机 (Phase 4) |
| KEY2 | PB5 | 预留 | — |

触摸失灵时（湿手、手套）物理按键做可靠后备。

### 4.6 手势分发逻辑

```c
void gesture_dispatch(Touch_Gesture_t g)
{
    page_state_t state = page_manager_get_state();

    switch (state) {

    case STATE_AT_HUB:
        if (g == GESTURE_LEFT)       page_manager_go_spoke(SPOKE_LEFT);
        else if (g == GESTURE_RIGHT) page_manager_go_spoke(SPOKE_RIGHT);
        else if (g == GESTURE_UP)    page_manager_go_spoke(SPOKE_UP);
        else if (g == GESTURE_DOWN)  page_manager_go_spoke(SPOKE_DOWN);
        else if (g == GESTURE_LONGPRESS) page_manager_enter_watchface_sel();
        break;

    case STATE_AT_SPOKE:
        if (g != GESTURE_NONE)       page_manager_go_home();
        break;

    case STATE_AT_OVERLAY:
        if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
            page_manager_pop();
        break;
    }
}
```

---

## 5. 页面详细设计

### 5.1 表盘 (Hub)

```
┌──────────────────────────┐
│ 22:45:30                 │
│ 2026-05-23  Fri          │
│                          │
│                          │
│                          │
│       👣 8,432           │  ← 纯显示，不响应点击
│                          │
│  ████████████░░  85%     │
└──────────────────────────┘
```

**交互:**
- 短点击 → 无反应（表盘就是信息面板）
- 四向滑动 → 辐射到对应页面
- **长按 1s → 表盘切换模式**: 表盘缩小，左右滑实时预览表盘样式，点一下或 5s 不动确认

**数据来源:** DataProvider，1s 刷新

---

### 5.2 心率页 (← 左辐射)

```
┌──────────────────────────┐
│ 心率                     │
│                          │
│         ♥ 75             │
│        bpm               │
│                          │
│  ┌──┬──┬──┬──┬──┐      │
│  │热身│燃脂│有氧│极限│   │  ← 区间指示，当前区间高亮
│  └──┴──┴──┴──┴──┘      │
│                          │
│  静息心率: 62            │
└──────────────────────────┘
```

- MAX30102 后台 HR 模式持续跑，划过来就有数据
- 1s 刷新
- 无子页

---

### 5.3 血氧页 (→ 右辐射)

```
┌──────────────────────────┐
│ 血氧 SpO₂               │
│                          │
│        ┌──────┐         │
│        │ 98%  │         │  ← 最近一次结果
│        │ 15:30 │         │
│        └──────┘         │
│                          │
│      [ 开始测量 ]        │  ← 点击触发 30s 测量
│                          │
│      历史记录 〉         │  → push 血氧历史
└──────────────────────────┘
```

- 平时显示最近一次结果，- 表示无数据
- 点"开始测量" → MAX30102 切 SpO2 模式 30s → 显示进度 + 结果
- 可以再测（覆盖上次结果）

---

### 5.4 控制中心 (↑ 上辐射)

```
┌──────────────────────────┐
│                          │
│   🔆       🔕       ⏱   │
│  亮度    勿扰    秒表    │
│                          │
│   👣       🧮       ⏰   │
│  活动    计算器   闹钟   │
│                          │
│ ───────────────────────  │
│            ⚙ 菜单     〉  │
└──────────────────────────┘
```

- 3×2 图标网格 + 底部菜单入口
- 每个图标 ~70×75px，手指不误触
- 点击图标 push 对应子页
- **亮度图标交互:**

```
点击 🔆
  │
  ▼ 弹出 overlay (不跳页)
┌──────────────────────────┐
│ 🔆(A) ████████████░░     │
│  ↑    亮度滑动条          │
│       自动=橙色跟随 lux   │
│       手动=灰色可拖动     │
│                          │
│ 点太阳图标 手动⇄自动      │
│ 点空白处收起             │
└──────────────────────────┘
```

  自动模式: 太阳橙色，滑块跟 BH1750 lux 值实时走，不可拖
  手动模式: 太阳灰色，滑块 5 档可拖
  自动亮度 range: 最低 10% / 最高 100%

---

### 5.5 通知页 (↓ 下辐射)

```
┌──────────────────────────┐
│ 通知                     │
│                          │
│  (暂无消息)              │  ← Phase 3 接入前占位
│                          │
│  接BLE后显示:             │
│  来电/短信/微信/App推送   │
└──────────────────────────┘
```

- Phase 3 接入 ESP32-C3 BLE 后填充
- 每条消息: 图标 + 标题 + 时间
- 点击可展开摘要

---

### 5.6 菜单 (从控制中心 ⚙ push)

```
┌──────────────────────────┐
│ 菜单                     │
│ ───────────────────────  │
│ 显示与亮度          〉   │
│ 表盘样式            〉   │
│ 蓝牙                〉   │  (Phase 3)
│ 闹钟                〉   │
│ 活动详情            〉   │
│ 运动记录            〉   │
│ 血氧历史            〉   │
│ 秒表                〉   │
│ 计时器              〉   │
│ 计算器              〉   │
│ 找手机              〉   │  (Phase 4)
│ 关于设备            〉   │
└──────────────────────────┘
```

12 个列表项，每项 push 对应子页。

---

### 5.7 显示与亮度 (Overlay)

```
┌──────────────────────────┐
│ 显示与亮度               │
│                          │
│ 🔆(A) ████████████░░     │  ← 同控制中心的亮度 overlay
│                          │
│ 熄屏时间                 │
│ ○ 5s  ○ 10s ○ 30s ○ 常亮│
└──────────────────────────┘
```

---

### 5.8 表盘样式 (Overlay + 长按入口)

```
长按表盘 1s 或 菜单 → 表盘样式

┌──────────────────────────┐
│   [当前表盘实时预览]      │  ← 左右滑实时切换
│                          │
│  ○ 经典数字              │
│  ○ 简约指针              │
│  ○ 大字极简              │
│                          │
│ 点一下确认 / 5s不动自动确认│
└──────────────────────────┘
```

---

### 5.9 活动详情 (Overlay)

```
┌──────────────────────────┐
│ 今日活动                 │
│                          │
│  👣 8,432 步             │
│  🔥 320 kcal            │
│  📏 5.8 km              │
│                          │
│ 本周趋势                  │
│  [简单柱状图]             │
└──────────────────────────┘
```

---

### 5.10 压栈页速查表

| 页面 | 内容 |
|------|------|
| 闹钟列表 | 最多 3 组闹钟，添加/编辑/删除，设置重复 |
| 秒表 | 大号计时数字，开始/计次/重置 |
| 计时器 | 设定时/分/秒，开始/暂停，到时提醒 |
| 计算器 | 基础四则运算，LCD 风格数字显示 |
| 血氧历史 | 每次测量记录列表: 时间 + 数值 |
| 运动记录 | 历史运动列表: 类型/时长/消耗 |
| 找手机 | 确认触发页 (Phase 4) |
| 蓝牙 | 连接状态/设备列表 (Phase 3) |
| 关于设备 | 固件版本/存储信息/OTA状态 |

---

## 6. 页面总数汇总

| 类型 | 数量 | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
|------|------|---------|---------|---------|---------|
| Hub | 1 | 1 | — | — | — |
| Spoke | 4 | 2 (心率+控制中心) | 1 (血氧) | 1 (通知) | — |
| Overlay | 13 | 5 (菜单/显示/表盘样式/活动/关于) | 6 (闹钟/秒表/计时器/计算器/血氧历史/运动记录) | 1 (蓝牙) | 1 (找手机) |
| **合计** | **18** | **8** | **7** | **2** | **1** |

---

## 7. 数据提供层 (Data Provider)

### 7.1 全量接口

```c
// data_provider.h — 所有页面只依赖这一个头文件

/* ── 时间 (DS3231) ── */
typedef struct {
    int hour, min, sec;
    int year, month, day;
    int weekday;  // 0=Sun
} watch_time_t;

void watch_data_get_time(watch_time_t *t);

/* ── 心率 (MAX30102, 后台HR模式持续采集) ── */
int32_t watch_data_get_heart_rate(void);   // bpm, -1=无数据

/* ── 血氧 (MAX30102, SpO2模式手动触发, 30s) ── */
int32_t watch_data_get_spo2(void);         // %, -1=未测
void    watch_data_spo2_start(void);       // 开始测量
bool    watch_data_spo2_is_done(void);     // 测量完成?
int32_t watch_data_spo2_history_count(void);
bool    watch_data_spo2_history_get(int32_t idx, int32_t *spo2, watch_time_t *t);

/* ── 活动 (QMI8658 IMU 后台计步) ── */
int32_t watch_data_get_steps(void);
int32_t watch_data_get_calories(void);
int32_t watch_data_get_distance_m(void);

/* ── 运动 ── */
typedef enum { WORKOUT_NONE, WORKOUT_RUN, WORKOUT_WALK, WORKOUT_CYCLE } workout_type_t;
typedef struct {
    workout_type_t type; int32_t duration_sec;
    int32_t avg_hr, steps, calories, distance_m; bool is_active;
} workout_session_t;

void watch_data_workout_start(workout_type_t type);
void watch_data_workout_pause(void);
void watch_data_workout_resume(void);
void watch_data_workout_stop(void);
void watch_data_workout_get(workout_session_t *s);
int32_t watch_data_workout_history_count(void);
bool    watch_data_workout_history_get(int32_t idx, workout_session_t *s);

/* ── 环境 (BME280, 10s周期) ── */
int32_t watch_data_get_temperature(void);  // °C × 10, 如 225 = 22.5°C
int32_t watch_data_get_humidity(void);     // %
int32_t watch_data_get_pressure(void);     // hPa
int32_t watch_data_get_altitude(void);     // m

/* ── 环境光 (BH1750, 500ms周期) ── */
int32_t watch_data_get_ambient_lux(void);
bool    watch_data_is_auto_brightness(void);
void    watch_data_set_auto_brightness(bool on);
void    watch_data_set_brightness(int32_t pct);    // 手动 0-100
int32_t watch_data_get_brightness(void);

/* ── 系统 ── */
int32_t watch_data_get_battery(void);
int32_t watch_data_get_screen_timeout(void);
void    watch_data_set_screen_timeout(int32_t sec);

/* ── 表盘样式 ── */
typedef enum { WATCHFACE_CLASSIC, WATCHFACE_ANALOG, WATCHFACE_MINIMAL } watchface_t;
watchface_t watch_data_get_watchface(void);
void        watch_data_set_watchface(watchface_t wf);
int32_t     watch_data_watchface_count(void);

/* ── 闹钟 ── */
typedef struct { int hour, min; bool enabled; bool repeat[7]; } watch_alarm_t;
int32_t watch_data_alarm_count(void);
bool    watch_data_alarm_get(int32_t idx, watch_alarm_t *a);
void    watch_data_alarm_set(int32_t idx, const watch_alarm_t *a);

/* ── 通知 (Phase 3) ── */
typedef struct { char title[32], body[100]; uint32_t timestamp; bool unread; } watch_notification_t;
int32_t watch_data_notification_count(void);
bool    watch_data_notification_get(int32_t idx, watch_notification_t *n);
void    watch_data_notification_mark_read(int32_t idx);
void    watch_data_notification_clear_all(void);
bool    watch_data_notification_has_new(void);

/* ── OTA (Phase 4) ── */
bool watch_data_ota_available(void);
int  watch_data_ota_progress(void);
void watch_data_ota_trigger(void);

/* ── 找手机 (Phase 4) ── */
void watch_data_find_phone_trigger(void);
```

### 7.2 实现切换

```c
// data_provider.c
#define USE_FAKE_DATA  1

#if USE_FAKE_DATA
    int32_t watch_data_get_heart_rate(void) { return 75; }
    int32_t watch_data_get_spo2(void)      { return 98; }
    int32_t watch_data_get_steps(void)     { return 8432; }
    // ...
#else
    int32_t watch_data_get_heart_rate(void) { return max30102_read_hr(); }
    // ...
#endif
```

**未实现的接口** (如 Phase 3 的通知、Phase 4 的 OTA) 返回安全默认值 (-1 / 0 / false)。页面直接调用，不需要判断。

---

## 8. 页面接口契约

```c
// page_t — 每个页面需要实现的结构体

typedef enum { PAGE_TYPE_HUB, PAGE_TYPE_SPOKE, PAGE_TYPE_OVERLAY } page_type_t;

typedef struct {
    const char  *name;
    page_type_t  type;
    lv_obj_t   *(*create)(lv_obj_t *parent);
    void        (*destroy)(void);
    void        (*update)(void);
} page_t;
```

**页面模板:**

```c
static lv_timer_t *refresh_timer;

static void on_refresh(lv_timer_t *timer) {
    // 从 data_provider 读数据 → 更新 LVGL 控件
}

lv_obj_t *page_xxx_create(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    // ... 创建子控件 ...
    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

void page_xxx_destroy(void) {
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

const page_t page_xxx = {
    .name = "xxx", .type = PAGE_TYPE_OVERLAY,
    .create = page_xxx_create, .destroy = page_xxx_destroy,
};
```

---

## 9. RTOS 任务规划

| Phase | 任务数 | 列表 |
|-------|--------|------|
| P1 | 2 | `LvglTask` (4KB, LVGL+app_loop) + `defaultTask` (512B, 空闲) |
| P2 | 3 | + `SensorTask` (2KB, 100ms 采集所有传感器, 写入 DataProvider 缓冲区) |
| P3 | 3 | SensorTask 扩展处理 BLE 消息 |
| P4 | 3 | OTA 由 Bootloader 接管, App 任务不变 |

---

## 10. Flash 分区 (Phase 4, OTA)

```
内部 Flash (512KB):
┌──────────────────────┐ 0x08000000
│  Bootloader (32KB)   │  校验 + 烧写 + 跳转
├──────────────────────┤ 0x08008000  ← VTOR
│  App (480KB)         │  主固件
└──────────────────────┘ 0x08080000

外部 W25Q64 (8MB):
┌──────────────────────┐ 0x000000
│  OTA 缓存 (512KB)    │  新固件暂存
├──────────────────────┤
│  文件系统 (7.5MB)    │  运动记录 / 配置
└──────────────────────┘
```

---

## 11. 开发路线图

### Phase 1: UI 框架 (当前)

**目标:** 8 个核心页面跑通，辐射导航工作，假数据刷新正常

| 文件 | 说明 |
|------|------|
| `App/` 目录结构 | 3 个子目录 + app.c |
| `Data/data_provider` | 全量接口 + USE_FAKE_DATA |
| `Framework/page_manager` | Hub/Spoke/Overlay 三态管理 |
| `Framework/gesture` | CST816 四向 + 长按分发 |
| `Pages/page_watchface` | 表盘 (时间/步数/电池 + 长按切换) |
| `Pages/page_heartrate` | 心率页 (数字+区间) |
| `Pages/page_control_center` | 控制中心 (6图标+菜单入口) |
| `Pages/page_menu` | 菜单列表 |
| `Pages/page_display` | 显示与亮度 (自动BH1750/手动/熄屏) |
| `Pages/page_watchface_sel` | 表盘样式选择 (左右滑预览) |
| `Pages/page_activity` | 活动详情 |
| `Pages/page_about` | 关于设备 |
| `freertos.c` 对接 | 替换测试代码为 app_init/app_loop |
| `eide.yml` 更新 | 添加所有 App 文件 |

### Phase 2: 传感器 + 健康功能

- BSP: QMI8658 / MAX30102 / BME280 / BH1750 / DS3231 驱动
- DataProvider 切换真实传感器 (`USE_FAKE_DATA = 0`)
- RTOS 新增 SensorTask
- Pages: 血氧 / 闹钟 / 秒表 / 计时器 / 计算器 / 血氧历史 / 运动记录

### Phase 3: BLE + 智能功能

- ESP32-C3 固件 + 手机 App (基础版)
- Pages: 通知 / 蓝牙
- 抬腕亮屏 (QMI8658 INT)、久坐提醒

### Phase 4: OTA + 产品化

- Bootloader 独立工程
- W25Q64 驱动 + 文件系统
- Pages: 找手机
- 省电策略 (熄屏/降帧/STOP)

---

## 12. 与 OV_Watch 对比

| 方面 | OV_Watch | XiYangWatch |
|------|----------|-------------|
| LVGL | 8.2 | 9.5 |
| 导航模型 | 纯栈 (push/pop) | 辐射式 (Hub-and-Spoke) |
| 页面数 | ~20 | 18 (一次规划) |
| 任务数 | 13 | 2→3 |
| 数据抽象 | 全局 struct + 函数指针 | 纯函数 + `#if` 编译期切换 |
| UI 方式 | SquareLine Studio | 手写 |
| 字体 | 大量中文字体 | 1bpp 按需生成 |
| Bootloader | 无 | 独立工程, OTA 支持 |
| 自动亮度 | 无 | BH1750 真实自适应 |
| 长按交互 | 无 | 长按切换表盘 |
| 接口预留 | 无 | 全量接口 + 安全默认值 |
| 文档 | 无 | Docs/ 体系 |
| 内存 | ~90KB | ~58KB (优化后) |

---

## 附录: 命名速查表

| 前缀 | 含义 | 示例 |
|------|------|------|
| `page_` | App/Pages/ 页面 | `page_watchface` |
| `watch_data_` | App/Data/ 接口 | `watch_data_get_heart_rate` |
| `page_manager_` | 页面管理器 | `page_manager_go_home` |
| `gesture_` | 手势分发 | `gesture_dispatch` |
| `app_` | 应用入口 | `app_init`, `app_loop` |
