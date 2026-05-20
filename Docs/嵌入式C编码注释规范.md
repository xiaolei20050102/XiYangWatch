# 嵌入式 C 编码注释规范

> 适用平台：STM32F4 + HAL + FreeRTOS
> 参考标准：Doxygen、ST HAL 库风格、MISRA C、Google C++ Style Guide

---

## 1. 文件头注释

每个 `.c` 和 `.h` 文件顶部必须包含文件头注释，说明文件的归属、功能和修改记录。

```c
/**
 * @file    lcd_init.c
 * @author  Your Name <email@example.com>
 * @brief   ST7789 LCD 驱动初始化，含 GPIO 配置与 SPI 读写
 * @date    2025-01-15
 * @version v1.0
 *
 * @details 支持硬件 SPI 与软件 SPI 切换，适配 240x280 竖屏/横屏。
 *          依赖 HAL 库 GPIO 与 SPI 模块。
 *
 * @note    横屏模式通过 USE_HORIZONTAL 宏切换（0/1=竖屏, 2/3=横屏）
 *
 * @par 修改记录:
 * - 2025-01-15  v1.0  创建文件，实现基本初始化
 * - 2025-03-02  v1.1  增加 DMA 写屏支持
 * - 2025-05-10  v1.2  修复 SleepIn 时序问题
 */
```

**关键点：**
- `@file`：文件名（IDE 中可随重命名自动更新）
- `@brief`：一句话说清这个文件是干什么的
- `@details`：详细信息，说明依赖、硬件平台、通信协议
- `@note`：注意事项、已知限制、配置宏说明
- `@par 修改记录`：每次修改的日期、版本、变更内容

---

## 2. 文件内部分区

按功能模块分块，每个区域用统一的分隔符标记。顺序固定：

```c
/* ========================== 头文件包含 ========================== */

#include "lcd_init.h"
#include "spi.h"
#include "delay.h"

/* ========================== 宏定义 ========================== */

#define LCD_TIMEOUT_MS      1000U   /**< SPI 传输超时时间 */
#define PWM_PERIOD          1000U   /**< PWM 周期(定时器重载值) */
#define LCD_BRIGHTNESS_MAX  100U    /**< 最大亮度百分比 */

/* ========================== 类型 / 枚举定义 ========================== */

/** @brief LCD 旋转方向枚举 */
typedef enum {
    LCD_ROTATE_0   = 0,   /**< 不旋转 */
    LCD_ROTATE_90  = 1,   /**< 顺时针 90° */
    LCD_ROTATE_180 = 2,   /**< 顺时针 180° */
    LCD_ROTATE_270 = 3,   /**< 顺时针 270° */
} LCD_Rotate_t;

/* ========================== 局部变量 ========================== */

/** @brief LCD 设备上下文(句柄) */
static LCD_Dev_t g_lcdDev;

/* ========================== 局部函数声明 ========================== */

static void LCD_WriteCmd(uint8_t cmd);
static void LCD_WriteData(uint8_t data);

/* ========================== 局部函数实现 ========================== */

/** @brief 写入命令(CS 拉低 -> DC=0 -> 发数据 -> CS 拉高) */
static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_CS_Clr();
    LCD_DC_Clr();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, LCD_TIMEOUT_MS);
    LCD_CS_Set();
}

/* ========================== 全局函数实现 ========================== */

/**
 * @brief   LCD 初始化（GPIO + 复位 + 配置寄存器序列）
 * @param   无
 * @retval  无
 * @note    上电后需先调用此函数才能使用屏幕，配置期间关闭全局中断
 */
void LCD_Init(void)
{
    // ...
}
```

**区域顺序（必须遵守，不要打乱）：**

| 顺序 | 区域 | 分隔符 |
|------|------|--------|
| 1 | 文件头注释 | `@file` Doxygen 块 |
| 2 | 头文件包含 | `/* ===== 头文件包含 ===== */` |
| 3 | 宏定义 | `/* ===== 宏定义 ===== */` |
| 4 | 类型/枚举/结构体定义 | `/* ===== 类型 / 枚举定义 ===== */` |
| 5 | 全局变量 | `/* ===== 全局变量 ===== */` |
| 6 | 局部变量 | `/* ===== 局部变量 ===== */` |
| 7 | 局部函数声明 | `/* ===== 局部函数声明 ===== */` |
| 8 | 局部函数实现 | `/* ===== 局部函数实现 ===== */` |
| 9 | 全局函数实现 | `/* ===== 全局函数实现 ===== */` |

**为什么这个顺序重要？**
- 编译器从前往后读，前面的代码看不到后面的声明
- 模块静态函数放前面，全局 API 放最后 → 读代码时先读内部再读对外接口，符合"自底向上"的阅读逻辑
- 修改时可以直接定位到对应区域

---

## 3. 头文件模板 (.h)

```c
/**
 * @file    lcd_init.h
 * @brief   ST7789 LCD 驱动初始化（对外接口）
 * @date    2025-01-15
 */

#ifndef __LCD_INIT_H__
#define __LCD_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ========================== 头文件包含 ========================== */

#include "sys.h"

/* ========================== 宏定义 ========================== */

/** @brief 屏幕方向：0/1=竖屏, 2/3=横屏 */
#define USE_HORIZONTAL  0

#if (USE_HORIZONTAL == 0) || (USE_HORIZONTAL == 1)
    #define LCD_W  240
    #define LCD_H  280
#else
    #define LCD_W  280
    #define LCD_H  240
#endif

/* ========================== 外设引脚宏定义 ========================== */

/** @name SPI 接口引脚
 *  @{ */
#define LCD_SCLK_PORT   GPIOB
#define LCD_SCLK_PIN    GPIO_PIN_3
#define LCD_SDA_PORT    GPIOB
#define LCD_SDA_PIN     GPIO_PIN_5
/** @} */

/** @name 控制引脚
 *  @{ */
#define LCD_RES_PORT    GPIOB
#define LCD_RES_PIN     GPIO_PIN_7
#define LCD_DC_PORT     GPIOB
#define LCD_DC_PIN      GPIO_PIN_9
#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_PIN_8
#define LCD_BLK_PORT    GPIOB
#define LCD_BLK_PIN     GPIO_PIN_0
/** @} */

/* ========================== 引脚操作宏 ========================== */

#define LCD_SCLK_Clr()  HAL_GPIO_WritePin(LCD_SCLK_PORT, LCD_SCLK_PIN, GPIO_PIN_RESET)
#define LCD_SCLK_Set()  HAL_GPIO_WritePin(LCD_SCLK_PORT, LCD_SCLK_PIN, GPIO_PIN_SET)

#define LCD_RES_Clr()   HAL_GPIO_WritePin(LCD_RES_PORT,  LCD_RES_PIN,  GPIO_PIN_RESET)
#define LCD_RES_Set()   HAL_GPIO_WritePin(LCD_RES_PORT,  LCD_RES_PIN,  GPIO_PIN_SET)

#define LCD_DC_Clr()    HAL_GPIO_WritePin(LCD_DC_PORT,   LCD_DC_PIN,   GPIO_PIN_RESET)
#define LCD_DC_Set()    HAL_GPIO_WritePin(LCD_DC_PORT,   LCD_DC_PIN,   GPIO_PIN_SET)

#define LCD_CS_Clr()    HAL_GPIO_WritePin(LCD_CS_PORT,   LCD_CS_PIN,   GPIO_PIN_RESET)
#define LCD_CS_Set()    HAL_GPIO_WritePin(LCD_CS_PORT,   LCD_CS_PIN,   GPIO_PIN_SET)

#define LCD_BLK_Clr()   HAL_GPIO_WritePin(LCD_BLK_PORT,  LCD_BLK_PIN,  GPIO_PIN_RESET)
#define LCD_BLK_Set()   HAL_GPIO_WritePin(LCD_BLK_PORT,  LCD_BLK_PIN,  GPIO_PIN_SET)

/* ========================== 全局函数声明 ========================== */

void LCD_GPIO_Init(void);
void LCD_Writ_Bus(uint8_t dat);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t dat);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Init(void);
void LCD_Set_Light(uint8_t dc);
void LCD_Close_Light(void);
void LCD_Open_Light(void);
void LCD_ST7789_SleepIn(void);
void LCD_ST7789_SleepOut(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_INIT_H__ */
```

---

## 4. 源文件模板 (.c)

```c
/**
 * @file    lcd_init.c
 * @brief   ST7789 LCD 驱动初始化，含 GPIO 配置与 SPI 读写
 * @date    2025-01-15
 * @version v1.0
 */

/* ========================== 头文件包含 ========================== */

#include "lcd_init.h"
#include "delay.h"
#include "spi.h"
#include "tim.h"

/* ========================== 宏定义 ========================== */

/** @brief PWM 分辨率(ARR 值) */
#define PWM_PERIOD  1000U

/* ========================== 局部函数声明 ========================== */

static void LCD_ResetSequence(void);

/* ========================== 全局函数实现 ========================== */

/**
 * @brief   LCD GPIO 引脚初始化（推挽输出，高速模式）
 * @param   无
 * @retval  无
 * @note    仅初始化 RES、CS、DC 三根控制线；SCLK/SDA 由 SPI 外设管理
 */
void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef gpioInit = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpioInit.Pin   = LCD_RES_PIN | LCD_CS_PIN | LCD_DC_PIN;
    gpioInit.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull  = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &gpioInit);
    HAL_GPIO_WritePin(GPIOB, LCD_RES_PIN | LCD_CS_PIN | LCD_DC_PIN, GPIO_PIN_SET);
}

/**
 * @brief   通过硬件 SPI 写入一个字节
 * @param   dat  待发送的 8-bit 数据
 * @retval  无
 */
void LCD_Writ_Bus(uint8_t dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, 100);
}

/**
 * @brief   写指令寄存器（DC=0 时写入为命令）
 * @param   dat  指令码
 * @retval  无
 */
void LCD_WR_REG(uint8_t dat)
{
    LCD_DC_Clr();
    LCD_Writ_Bus(dat);
    LCD_DC_Set();
}

/**
 * @brief   写一个字节数据（DC=1 时写入为数据）
 * @param   dat  8-bit 数据
 * @retval  无
 */
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_Writ_Bus(dat);
}

/**
 * @brief   写两个字节数据（16-bit 颜色值，如 RGB565）
 * @param   dat  16-bit 数据
 * @retval  无
 */
void LCD_WR_DATA(uint16_t dat)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(dat >> 8);
    buf[1] = (uint8_t)(dat & 0xFF);
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);
}

/**
 * @brief   设置显存写入窗口（行/列起止地址）
 * @param   x1  列起始地址
 * @param   y1  行起始地址
 * @param   x2  列结束地址
 * @param   y2  行结束地址
 * @retval  无
 */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_WR_REG(0x2A);           /* 列地址设置 */
    LCD_WR_DATA(x1);
    LCD_WR_DATA(x2);
    LCD_WR_REG(0x2B);           /* 行地址设置 */
    LCD_WR_DATA(y1);
    LCD_WR_DATA(y2);
    LCD_WR_REG(0x2C);           /* 存储器写 */
}

/**
 * @brief   设置背光亮度（PWM 占空比调节）
 * @param   dc  亮度百分比，范围 5 ~ 100
 * @retval  无
 * @note    值超出范围则直接返回不做操作
 */
void LCD_Set_Light(uint8_t dc)
{
    if ((dc < 5) || (dc > 100)) {
        return;
    }
    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, dc * PWM_PERIOD / 100);
}

/**
 * @brief   关闭背光（占空比设为 0）
 * @param   无
 * @retval  无
 */
void LCD_Close_Light(void)
{
    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, 0);
}

/**
 * @brief   开启背光（启动 PWM 输出）
 * @param   无
 * @retval  无
 */
void LCD_Open_Light(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

/**
 * @brief   ST7789 进入休眠模式
 * @param   无
 * @retval  无
 * @note    休眠后屏幕关闭，功耗约 10μA；唤醒请调用 LCD_ST7789_SleepOut()
 */
void LCD_ST7789_SleepIn(void)
{
    LCD_WR_REG(0x10);
    delay_ms(100);
}

/**
 * @brief   ST7789 退出休眠模式
 * @param   无
 * @retval  无
 * @note    退出后需等待 100ms 才能正常显示
 */
void LCD_ST7789_SleepOut(void)
{
    LCD_WR_REG(0x11);
    delay_ms(100);
}

/* ========================== 局部函数实现 ========================== */

/**
 * @brief   LCD 硬件复位时序
 * @note    RES 拉低 100ms -> 拉高 100ms，等待电源稳定
 */
static void LCD_ResetSequence(void)
{
    LCD_RES_Clr();
    delay_ms(100);
    LCD_RES_Set();
    delay_ms(100);
}

/**
 * @brief   LCD 完整初始化流程：复位 + 寄存器配置序列
 * @param   无
 * @retval  无
 * @note    包含伽马校正、显示方向、色深等全部寄存器配置
 */
void LCD_Init(void)
{
    LCD_GPIO_Init();
    LCD_CS_Clr();

    /* ---- 硬件复位 ---- */
    LCD_ResetSequence();

    /* ---- 退出休眠 ---- */
    LCD_WR_REG(0x11);
    delay_ms(120);

    /* ---- 显示方向 ---- */
    LCD_WR_REG(0x36);
    if      (USE_HORIZONTAL == 0) { LCD_WR_DATA8(0x00); }
    else if (USE_HORIZONTAL == 1) { LCD_WR_DATA8(0xC0); }
    else if (USE_HORIZONTAL == 2) { LCD_WR_DATA8(0x70); }
    else                          { LCD_WR_DATA8(0xA0); }

    /* ---- 像素格式：16-bit (RGB565) ---- */
    LCD_WR_REG(0x3A);
    LCD_WR_DATA8(0x05);

    /* ---- 显示开启 ---- */
    LCD_WR_REG(0x21);
    LCD_WR_REG(0x29);
}
```

---

## 5. 函数注释规范（Doxygen 格式）

### 5.1 标准模板

```c
/**
 * @brief   <一句话功能描述>
 * @param   <参数名>  <参数说明>
 * @param   <参数名>  <参数说明>
 * @retval  <返回值说明>
 * @note    <注意事项 / 调用限制 / 耗时说明>
 * @warning <危险操作警告>
 * @see     <相关联的函数>
 *
 * @par 使用示例:
 * @code
 * uint16_t color = LCD_GetPixel(100, 50);
 * @endcode
 */
```

### 5.2 各种函数场景的写法

**无参数无返回值：**
```c
/**
 * @brief   关闭背光
 * @retval  无
 */
void LCD_Close_Light(void);
```

**有参数有返回值：**
```c
/**
 * @brief   读取温湿度传感器数据
 * @param   humi   [出] 湿度值指针（单位: %RH，已除以10，如 452 = 45.2%）
 * @param   temp   [出] 温度值指针（单位: °C，已除以10，如 235 = 23.5°C）
 * @retval  0  读取成功
 * @retval  1  传感器超时无应答
 * @retval  2  CRC 校验失败
 * @note    调用前需先调用 AHT_Init() 初始化；本函数阻塞约 80ms
 */
uint8_t AHT_Read(float *humi, float *temp);
```

**带传入/传出标记：**
```c
/**
 * @brief   将缓冲区数据通过 DMA 发送至 LCD
 * @param   pData   [入] 像素数据缓冲区首地址
 * @param   len     [入] 数据长度（像素数）
 * @param   pSent   [出] 实际发送字节数（可为 NULL 表示不关心）
 * @retval  0   发送成功
 * @retval  -1  DMA 忙，发送失败
 */
int LCD_DMA_Send(const uint16_t *pData, uint32_t len, uint32_t *pSent);
```

**中断回调函数：**
```c
/**
 * @brief   SPI DMA 传输完成回调
 * @param   hspi  触发回调的 SPI 句柄
 * @retval  无
 * @note    在中断上下文中执行，不可调用阻塞函数，不可 printf
 * @warning 此函数需尽可能短，复杂处理请用信号量通知任务线程
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
```

**FreeRTOS 任务函数：**
```c
/**
 * @brief   LCD 刷新任务
 * @param   arg  任务参数（未使用）
 * @retval  无
 * @note    栈大小：512 words，优先级：3
 *          收到 DMA_Semaphore 后触发一次全屏刷新
 */
void Task_LCD_Refresh(void *arg);
```

### 5.3 `@param` 标记规范

| 标记 | 含义 | 使用场景 |
|------|------|----------|
| `[入]` | 输入参数，只读 | 函数读取此参数 |
| `[出]` | 输出参数，写入 | 函数写入此参数（指针） |
| `[入/出]` | 输入输出 | 函数读写，如结构体配置 |

---

## 6. 变量与常量的注释

### 6.1 全局变量

```c
/** @brief LCD 设备上下文（含分辨率、旋转、帧缓冲等状态） */
static LCD_Dev_t g_lcdDev;

/** @brief I2C 总线句柄（用于温湿度传感器 AHT21） */
iic_bus_t g_ahtBus = {
    .IIC_SDA_PORT = GPIOB,
    .IIC_SCL_PORT = GPIOB,
    .IIC_SDA_PIN  = GPIO_PIN_13,
    .IIC_SCL_PIN  = GPIO_PIN_14,
};
```

### 6.2 宏常量

```c
/** @brief 看门狗复位超时阈值(ms)，超过此时间未喂狗则系统复位 */
#define WDT_TIMEOUT_MS      5000U

/** @brief I2C 超时时间(tick)，约 100ms @ 1kHz tick */
#define I2C_TIMEOUT_TICKS   100U
```

### 6.3 枚举值

```c
/** @brief 系统电源状态 */
typedef enum {
    PWR_STATE_NORMAL   = 0,  /**< 正常运行 */
    PWR_STATE_LOW      = 1,  /**< 低电量（<20%） */
    PWR_STATE_CHARGE   = 2,  /**< 充电中 */
    PWR_STATE_SHUTDOWN = 3,  /**< 即将关机 */
} Power_State_t;
```

---

## 7. 行内注释规范

```c
/* ===== 好的行内注释 ===== */

/* ---- 硬件复位时序 ---- */
LCD_RES_Clr();
delay_ms(100);
LCD_RES_Set();
delay_ms(100);

LCD_WR_REG(0x36);           /* MADCTL: 显示方向控制 */
LCD_WR_DATA8(0x00);         /*   MY=0, MX=0, MV=0 → 竖屏不翻转 */

if (cnt == 0) {
    return 1;               /* 传感器超时，通信失败 */
}

/* ===== 避免的写法 ===== */

// 写寄存器              ← 无意义，寄存器值是什么？
LCD_WR_REG(0x36);

i++;  // i 加 1            ← 代码即文档，完全多余
```

**行内注释原则：**
- 解释 **为什么这样做**，不是 **做了什么**
- 寄存器地址写完后备注寄存器名
- 魔数必须注释含义
- 不写废话注释

---

## 8. 注释语言选择

| 场景 | 推荐语言 | 原因 |
|------|----------|------|
| Doxygen 块注释 (`@brief` 等) | 英文 | 配合 Doxygen 生成标准化文档 |
| 也可以 | 中文 | 团队内部方便阅读，你的当前风格 |
| 寄存器说明 | 英文 | 与数据手册对照更直接 |
| 业务逻辑注释 | 中文 | 团队沟通效率更高 |

> **建议：** 个人项目保持中文，与团队协作时协商统一。Doxygen 的 `@brief`/`@param` 用中文也完全可以，工具链支持 UTF-8。

---

## 9. 完整 .c 文件各部分注释密度参考

以 `lcd_init.c` 为例，一个好的注释密度大概是：

```
总行数: ~270 行
├── 文件头 Doxygen:  15 行  (有)
├── 分区标题行:      9 行   (有)
├── 宏注释:          1 行/个 (部分有)
├── 函数 Doxygen:    8 行/函数 (有，但可更规范)
├── 关键行内注释:    15 行  (有，比较充分)
└── 纯代码:          其余
```

注释率建议在 **15% ~ 25%** 之间，过低说明缺乏文档，过高说明代码本身可读性可能有问题。

---

## 10. IDE 快捷操作建议

| 你的 IDE (Keil MDK) | 操作 |
|---------------------|------|
| 函数注释模板 | 利用 Keil 的 Code Template 功能，绑定快捷键一键插入 `@brief/@param/@retval` 模板 |
| VSCode | 安装 Doxygen Documentation Generator 插件，输入 `/**` 后回车自动生成模板 |
| CLion | 内置 Doxygen 支持，`/**` + Enter 自动补全 `@param` |

---

## 附录A：快速自检清单

在提交代码前逐项检查：

- [ ] 每个 `.c` / `.h` 文件顶部有 `@file` + `@brief` 文件头注释
- [ ] 每个全局函数有 `@brief` + `@param` + `@retval` 注释
- [ ] 每个 `static` 函数有至少一行 `@brief` 注释（简单的可以不写 `@param`）
- [ ] 每个宏定义有 `/**< 说明 */` 行尾注释（或上方一行注释）
- [ ] 全局变量有 `/** @brief ... */` 注释
- [ ] 魔数（裸数字）都有注释说明含义
- [ ] 寄存器写入处标注了寄存器名
- [ ] 复杂逻辑段前有分隔短注释（`/* ---- xxx ---- */`）
- [ ] 分区标题格式统一（`/* ===== 标题 ===== */`）
- [ ] `@note` 标注了阻塞/中断限制/时序要求等关键信息

---

## 附录B：替换对照表

把你现有的注释风格改成规范格式：

| 现有写法 | 规范写法 |
|----------|----------|
| `/**************\n      函数说明：LCD初始化\n      入口数据：无\n      返回值：  无\n**************/` | `/** @brief LCD初始化 @retval 无 */` 或完整多行 Doxygen |
| `//SCL=SCLK` | 写在宏定义行尾：`/**< SCL 时钟线 */` |
| `//初始化GPIO` | `/** @brief GPIO 推挽输出初始化 */` |
| `//hard SPI` | `/* 使用硬件 SPI，软件 SPI 代码保留调试用 */` |
| `#define USE_HORIZONTAL 0  //设置显示方向` | `/** @brief 屏幕方向：0/1=竖屏, 2/3=横屏 */` |
