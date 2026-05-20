# STM32F411 引脚分配方案

> 基于你填写的模块信息，下面给出完整引脚分配。原则：
> - I2C1 挂 6 个传感器共用一条总线（面包板用电源轨走线）
> - I2C2 独立给 EEPROM
> - SPI1 底板 W25Q64 独占（PA5/PA6/PA7，已焊死）
> - SPI2 给 LCD ST7789（PB13/PB14/PB15），互不干扰

---

## 一、完整引脚分配表

| STM32 引脚 | 功能 | 接模块 | 备注 |
|------------|------|--------|------|
| **PA0** | GPIO IN | 底板 KEY 按键 | ⚠️ 已占用，输入上拉，低电平有效 |
| **PA1** | PWM (TIM2_CH2) | LCD PWR 背光 | 支持硬件 PWM 调光 |
| **PA2** | UART2 TX | 调试串口 RX | 接 USB-TTL 模块的 RX |
| **PA3** | UART2 RX | 调试串口 TX | 接 USB-TTL 模块的 TX |
| **PA4** | GPIO OUT | 底板 W25Q64 CS | ⚠️ 底板焊死，软件 CS（Demo 确认） |
| **PA5** | SPI1 SCK | 底板 W25Q64 CLK | 硬件 SPI 时钟 |
| **PA6** | SPI1 MISO | 底板 W25Q64 DO | 硬件 SPI 数据输入 |
| **PA7** | SPI1 MOSI | 底板 W25Q64 DI | 硬件 SPI 数据输出 |
| **PA8** | GPIO INT | QMI8658 INT1 | 外部中断，翻腕/数据就绪 |
| **PA9** | UART1 TX | ESP32-C3 RX | BLE 协处理器通信 |
| **PA10** | UART1 RX | ESP32-C3 TX | BLE 协处理器通信 |
| **PA11** | USB DM | — | ⚠️ 底板 USB 占用 |
| **PA12** | USB DP | — | ⚠️ 底板 USB 占用 |
| **PA13** | SWDIO | — | ⚠️ 调试占用 |
| **PA14** | SWCLK | — | ⚠️ 调试占用 |
| **PA15** | GPIO INT | MAX30102 INT | 外部中断，心率数据就绪 |
| **PB0** | GPIO OUT | LCD DC | 命令/数据切换 |
| **PB1** | GPIO OUT | LCD CS | 屏幕片选（软件 CS） |
| **PB2** | GPIO INT | CST816S TINT | 外部中断，触摸检测 |
| **PB3** | I2C2 SDA (AF4) | AT24C256 SDA | I2C2 数据（需禁用 JTAG，设置 AF4） |
| **PB4** | GPIO INT | DS3231 SQW | 闹钟中断（可选，不用则悬空） |
| **PB5** | GPIO INT | 外接按键 2 | 第二按键（PA0 为第一按键） |
| **PB6** | I2C1 SCL | ★ I2C 总线时钟 | 挂 6 个传感器（见下表） |
| **PB7** | I2C1 SDA | ★ I2C 总线数据 | 挂 6 个传感器（见下表） |
| **PB8** | — | 预留 | 暂空，后续可接马达 PWM |
| **PB9** | — | 预留 | 暂空 |
| **PB10** | I2C2 SCL | AT24C256 SCL | EEPROM 独立总线 |
| **PB11** | GPIO OUT | CST816S TRST | 触摸复位（从 PB3 迁出） |
| **PB12** | GPIO OUT | LCD RST | 屏幕复位（紧邻 SPI2 引脚） |
| **PB13** | SPI2 SCK | LCD SCK | 硬件 SPI 时钟 |
| **PB14** | SPI2 MISO | — | 悬空（LCD 不需读数据） |
| **PB15** | SPI2 MOSI | LCD SDA | 硬件 SPI 数据 |
| **PC13** | GPIO OUT | 底板 LED | ⚠️ 已占用，低电平点亮 |
| **PC14** | OSC32 IN | — | ⚠️ RTC 晶振占用 |
| **PC15** | OSC32 OUT | — | ⚠️ RTC 晶振占用 |

---

## 二、I2C1 总线设备清单（6个设备共享 PB6/PB7）

| 设备 | 7-bit 地址 | 地址来源 | 冲突？ |
|------|-----------|----------|--------|
| CST816S 触摸 | `0x15` | 固定 | — |
| QMI8658 IMU | `0x6B`（大概率）或 `0x6A` | AD0 接 GND=0x6B（出厂默认），悬空/接 VCC=0x6A | — |
| MAX30102 心率 | `0x57` | 固定 | — |
| BME280 环境 | `0x76` 或 `0x77` | SDO 引脚决定，接 GND=0x76 | — |
| BH1750 环境光 | `0x23` | ADDR 默认 4.7K 下拉=0x23 | — |
| DS3231 RTC | `0x68` | 固定 | — |

> 6 个地址全部不同，无冲突。面包板上把 6 个模块的 SCL 全部接到同一条轨、
> SDA 全部接到同一条轨，轨再接到 PB6/PB7 即可。

---

## 三、BME280 模式配置

BME280 模块同时支持 I2C 和 SPI，需要用引脚电平指定模式：

| 模块引脚 | 接法 | 说明 |
|----------|------|------|
| CSB | **接面包板 3.3V** | 高电平 = I2C 模式（不需占 GPIO） |
| SDO | **接面包板 GND** | 低电平 = 地址 0x76 |
| SCL | 接 I2C 总线 SCL | |
| SDA | 接 I2C 总线 SDA | |

---

## 四、底板 W25Q64

根据你提供的 Demo 代码，底板 W25Q64 走的是 **SPI1**：

| 信号 | STM32 引脚 | 说明 |
|------|-----------|------|
| SCK | PA5 | SPI1 时钟 |
| MISO | PA6 | SPI1 数据输入 |
| MOSI | PA7 | SPI1 数据输出 |
| CS | 见 demo 中 `F_CS_Pin` 定义 | 软件 CS |

> 查一下 Demo 里 `F_CS_Pin` 和 `F_CS_GPIO_Port` 的 `#define`，告诉我具体是哪个脚。

---

## 五、面包板接线速查

```
电源轨布局：
  左红(+) → 3.3V（全部模块 VCC）
  左蓝(-) → GND（全部模块 GND）
  右红(+) → PB6 / I2C1 SCL（全部 I2C 模块 SCL）
  右蓝(-) → PB7 / I2C1 SDA（全部 I2C 模块 SDA）

独走信号（不经过轨）：
  PB13, PB15, PB12, PB0, PB1, PA1 → ST7789 LCD（SPI2）
  PA4, PA5, PA6, PA7 → 底板 W25Q64（SPI1，已焊）
  PA2, PA3 → USB-TTL 调试串口
  PA9, PA10 → ESP32-C3
  PB10, PB11 → AT24C256（独立 I2C2）
  PA8, PA15, PB2, PB4 → 各传感器 INT 脚
```

---

## 六、确认状态

| # | 问题 | 状态 |
|---|------|------|
| 1 | W25Q64 CS | ✅ PA4（Demo 代码确认） |
| 2 | QMI8658 AD0 | ⚠️ 大概率接 GND → 地址 0x6B。拿万用表蜂鸣档测 AD0 ↔ GND 通断确认 |
| 3 | BH1750 ADDR | ✅ 保留默认下拉 → 0x23 |
| 4 | ESP32-C3 TX/RX | ✅ GPIO21(TX)、GPIO20(RX) |
| 5 | 电源/电池 | ✅ 暂用 USB 供电，电池后补 |
| 6 | 外部按键 | ✅ 2 个：PA0（底板 KEY）+ PB5（外接独立按键） |
