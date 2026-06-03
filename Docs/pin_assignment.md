# STM32F411 引脚分配方案

> 基于实际代码确认 (lcd_st7789.h, touch_cst816s.h, w25q64.h, CubeMX i2c.c, spi.c)。
> - I2C1 挂 5 个传感器共用一条总线
> - SPI1 给 LCD ST7789（PA5/PA6/PA7）
> - SPI2 给 W25Q64 Flash（PB13/PB14/PB15）

---

## 一、完整引脚分配表

| STM32 引脚 | 功能 | 接模块 | 备注 |
|------------|------|--------|------|
| **PA0** | GPIO IN | 底板 KEY 按键 | 输入上拉，低电平有效 |
| **PA1** | PWM (TIM2_CH2) | LCD PWR 背光 | 硬件 PWM 调光 |
| **PA2** | UART2 TX | 调试串口 RX | 接 USB-TTL / RTT (SEGGER) |
| **PA3** | UART2 RX | 调试串口 TX | — |
| **PA4** | GPIO OUT | W25Q64 CS | SPI2 Flash 片选 |
| **PA5** | SPI1 SCK | LCD SCK | 硬件 SPI 时钟 |
| **PA6** | SPI1 MISO | — | 悬空（LCD 只写不读） |
| **PA7** | SPI1 MOSI | LCD SDA | 硬件 SPI 数据 |
| **PA8** | GPIO INT | QMI8658 INT1 | 外部中断，翻腕/数据就绪 |
| **PA9** | UART1 TX | ESP32-C3 RX | BLE 协处理器通信 |
| **PA10** | UART1 RX | ESP32-C3 TX | BLE 协处理器通信 |
| **PA11** | USB DM | — | 底板 USB 占用 |
| **PA12** | USB DP | — | 底板 USB 占用 |
| **PA13** | SWDIO | — | 调试占用 |
| **PA14** | SWCLK | — | 调试占用 |
| **PA15** | GPIO INT | MAX30102 INT | 外部中断，心率数据就绪 |
| **PB0** | GPIO OUT | LCD DC | 命令/数据切换 |
| **PB1** | GPIO OUT | LCD CS | 屏幕片选（软件 CS） |
| **PB2** | GPIO INT | CST816S INT | 外部中断，触摸检测 |
| **PB3** | I2C2 SDA (AF4) | AT24C256 SDA | I2C2 数据（需禁用 JTAG） |
| **PB4** | GPIO INT | DS3231 SQW | 闹钟中断（可选） |
| **PB5** | GPIO INT | 外接按键 2 | 第二按键 |
| **PB6** | I2C1 SCL | ★ I2C 总线时钟 | 挂 5 个传感器 |
| **PB7** | I2C1 SDA | ★ I2C 总线数据 | 挂 5 个传感器 |
| **PB8** | GPIO OUT | CST816S RST | 触摸芯片复位引脚 |
| **PB9** | — | 预留 | 暂空 |
| **PB10** | I2C2 SCL | AT24C256 SCL | EEPROM 独立总线 |
| **PB11** | — | 预留 | 暂空 |
| **PB12** | GPIO OUT | LCD RST | 屏幕复位 |
| **PB13** | SPI2 SCK | W25Q64 CLK | 硬件 SPI 时钟 |
| **PB14** | SPI2 MISO | W25Q64 DO | 硬件 SPI 数据输入 |
| **PB15** | SPI2 MOSI | W25Q64 DI | 硬件 SPI 数据输出 |
| **PC13** | GPIO OUT | 底板 LED | 低电平点亮 |
| **PC14** | OSC32 IN | — | RTC 晶振占用 |
| **PC15** | OSC32 OUT | — | RTC 晶振占用 |

---

## 二、I2C1 总线设备清单（5 个设备共享 PB6/PB7）

| 设备 | 7-bit 地址 | 地址来源 | 代码路径 |
|------|-----------|----------|---------|
| CST816S 触摸 | `0x15` | 固定 | BSP/Touch/touch_cst816s.c |
| QMI8658 IMU | `0x6B` | AD0 接 GND | BSP/IMU/ (待建) |
| MAX30102 心率 | `0x57` | 固定 | BSP/HR/ (待建) |
| BME280 环境 | `0x76` | SDO 接 GND | BSP/ENV/ (待建) |
| DS3231 RTC | `0x68` | 固定 | BSP/RTC/ (待建) |

> 5 个地址全部不同，无冲突。

---

## 三、总线独占分配

| 总线 | STM32 外设 | 独占任务 | 接模块 |
|------|-----------|---------|--------|
| **SPI1** | PA5/PA6/PA7 | LvglTask | LCD ST7789 (CS=PB1, DC=PB0, RST=PB12) |
| **SPI2** | PB13/PB14/PB15 | SaveTask | W25Q64 Flash (CS=PA4) |
| **I2C1** | PB6/PB7 | I2CSensTask | 5 传感器 |
| **USART1** | PA9/PA10 | BtTask | ESP32-C3 BLE |

---

## 四、面包板接线速查

```
电源轨：
  左红(+) → 3.3V（全部模块 VCC）
  左蓝(-) → GND（全部模块 GND）
  右红(+) → PB6 / I2C1 SCL（全部 I2C 模块 SCL）
  右蓝(-) → PB7 / I2C1 SDA（全部 I2C 模块 SDA）

独走信号：
  PA5, PA7, PB1, PB0, PB12, PA1 → ST7789 LCD（SPI1）
  PB13, PB14, PB15, PA4 → W25Q64（SPI2）
  PB8, PB2 → CST816S
  PA9, PA10 → ESP32-C3
  PA2, PA3 → USB-TTL / RTT 调试
```
