# DMA + 双缓冲：从原理到实战

> 在 STM32F411 + ST7789 LCD + LVGL 手表项目中的优化实践

---

## 一、基础概念

### 1.1 什么是 DMA

DMA（Direct Memory Access，直接存储器访问）是一个**独立的硬件控制器**，能在不占用 CPU 的情况下，在内存和外设之间搬运数据。

**没有 DMA 时（CPU 轮询/中断）：**

```
CPU:  取一字节 → 写 SPI 数据寄存器 → 等 SPI 发完 → 取下一字节 → ...
      ████████████████████████████████████████████████████████
      全程被占用，无法做其他事
```

**有了 DMA：**

```
CPU:  配置 DMA（源地址、目标地址、字节数）→ 启动 DMA → 继续干别的事
DMA:  [从 SRAM 取] → [写 SPI] → [从 SRAM 取] → [写 SPI] → ...
      后台自动搬运
CPU:  渲染下一帧 / 处理触摸 / 计算...
```

核心价值：**CPU 和 I/O 并行工作**。

### 1.2 STM32F411 的 DMA 结构

STM32F411 有两个 DMA 控制器（DMA1、DMA2），每个有 8 个 Stream（数据流），每个 Stream 可配 8 个 Channel（通道）。

```
DMA1
├── Stream 0 ── Channel 0~7
├── Stream 1 ── Channel 0~7
├── ...
├── Stream 4 ── Channel 0  ← SPI2_TX 就在这里
└── Stream 7 ── Channel 0~7
```

关键配置项：

| 配置 | 含义 | 本项目取值 |
|------|------|-----------|
| Direction | 数据流向 | Memory → Peripheral（SRAM→SPI） |
| PeriphInc | 外设地址是否自增 | Disable（始终写 SPI->DR） |
| MemInc | 内存地址是否自增 | Enable（逐个字节读） |
| Mode | 工作模式 | Normal（发完即停，不用 Circular） |
| Data Width | 数据宽度 | Byte → Byte（SPI 8位模式） |
| FIFO | 是否用 FIFO 缓冲 | Disable（直通模式） |

### 1.3 DMA 中断链

DMA 发完指定字节数后，硬件触发中断：

```
DMA1_Stream4_IRQHandler          ← NVIC 中断向量
  └→ HAL_DMA_IRQHandler()       ← HAL 库处理 DMA 状态寄存器
       └→ XferCpltCallback()    ← HAL_SPI 内部回调
            └→ HAL_SPI_TxCpltCallback()  ← 用户层业务逻辑
```

用户可以覆盖 `HAL_SPI_TxCpltCallback`（原函数是 `__weak` 的），在里面做自定义处理。本项目用它来释放 LVGL 缓冲区。

---

## 二、双缓冲

### 2.1 为什么单缓冲不够

LVGL 用 PARTIAL 模式时，一次只渲染 N 行（本项目 20 行）。流程：

```
单缓冲:
  LVGL 渲染到 buf → disp_flush → DMA 发送 → 等 DMA 完成 → flush_ready
       ↑                                                         ↓
       └────────────────── buf 空闲，开始下一轮 ←─────────────────┘
```

问题：DMA 发送期间 CPU 在干什么？

- **优化前**（阻塞 SPI）：CPU 呆等，什么事情都干不了
- **优化后**（DMA）：CPU 空闲，但 LVGL 在等 `flush_ready`，还是不能渲染

DMA 释放了 CPU，但 LVGL 被自己的单缓冲阻塞了——这是瓶颈。

### 2.2 双缓冲如何工作

给 LVGL 两块等大的缓冲区：`buf_1` 和 `buf_2`。

```
双缓冲:
  buf_1(空闲) → LVGL 渲染 → disp_flush(buf_1) → DMA 开始发送 buf_1
                                                    ↓ (DMA 异步)
  buf_2(空闲) → LVGL 发现 buf_2 空着 → 直接渲染到 buf_2
                                                    ↓
                                              DMA 完成 → ISR → flush_ready(buf_1)
                                                    ↓
  buf_1(空闲) ← 释放                        buf_2 渲染完 → disp_flush(buf_2)
                                                    ↓
                                              等 DMA 空闲...
```

**关键机制：**

1. LVGL 内部记录哪块 buf 正在被 flush（通过 `dispatch_buf` 字段）
2. LVGL 发现某块 buf 正忙（`flush_ready` 还没被调），就自动选另一块空闲的
3. 两块都忙就等着——说明 CPU 太快、DMA 太慢，系统瓶颈在 SPI 带宽

**本质是乒乓**：CPU 和 DMA 各用一块，交替进行，互不等待。

### 2.3 本项目的数据流

```
┌──────────┐   px_map指向buf_1    ┌──────────────┐
│  LVGL    │ ──────────────────→ │  disp_flush  │
│  渲染到   │                     │  → dma_busy锁│
│  buf_1   │                     │  → 启动 DMA   │
└──────────┘                     └──────┬───────┘
                                        │ 异步返回
┌──────────┐                     ┌──────▼───────┐
│  LVGL    │                     │  DMA1_Stream4 │
│  渲染到   │                     │  buf_1 ──→ SPI│
│  buf_2   │                     │  后台搬运      │
└──────────┘                     └──────┬───────┘
                                        │ 发完中断
                               ┌────────▼──────────┐
                               │  DMA ISR           │
                               │  → CS↑ 结束写屏     │
                               │  → flush_ready     │
                               │  → buf_1 释放      │
                               └───────────────────┘
```

---

## 三、润色优化

### 3.1 SPI 时钟提速

| 分频 | SPI 时钟 | 一行(240px)耗时 |
|------|----------|----------------|
| SPI_BAUDRATEPRESCALER_8 | 6.25 MHz | ~307 μs |
| SPI_BAUDRATEPRESCALER_4 | 12.5 MHz | ~154 μs |
| **SPI_BAUDRATEPRESCALER_2** | **25 MHz** | **~77 μs** |

25MHz 是 APB1=50MHz 下的最高速率。实测无雪花，画面稳定。

### 3.2 缓冲大小取舍

| 缓冲行数 | 每缓冲大小 | 双缓冲合计 | 整帧 flush 次数 |
|----------|-----------|-----------|----------------|
| 10 行 | 4.8 KB | 9.6 KB | 28 次 |
| **20 行** | **9.6 KB** | **19.2 KB** | **14 次** |
| 40 行 | 19.2 KB | 38.4 KB | 7 次 |

20 行是最佳甜点：flush 次数减半，内存只多占 10KB，边际收益最大。40 行继续翻倍内存但减少的 flush 次数有限。

### 3.3 去掉中间拷贝

最初设计里有一个 `buf_DMA` 中间缓冲：`lv_memcpy(buf_DMA, px_map, size)` → `DMA 发送 buf_DMA`。

逻辑上这是多余的。原因：

- LVGL 在 `flush_ready` 之前绝不会碰 `px_map` 指向的 buf
- `dma_busy` 确保同一时间只有一个 DMA 在跑
- 两个保证加在一起，`px_map` 本身就是 DMA-安全的，直接发给 DMA 即可

省了 9.6KB 内存 + 每帧一次 memcpy。

---

## 四、关键代码解析

### 4.1 `disp_flush` — LVGL 的 flush 回调

```c
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area,
                       uint8_t * px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    while (dma_busy) {}   // [1] 等上一次 DMA 传输完成
    dma_busy = true;      // [2] 锁住 SPI 外设

    ST7789_FlushArea_DMA(area->x1, area->y1, w, h, px_map); // [3]
    // 注意：不调 lv_display_flush_ready！留给 DMA 中断回调
}
```

**注释：**

- [1] `dma_busy` 是 `volatile bool`，在 ISR 中置 false。忙等不会太久——20 行×240×16bit @25MHz ≈ 1.5ms
- [2] 必须在 DMA 之前置位，否则有竞态窗口
- [3] 直接传 `px_map`，不拷贝。`px_map` 就是 LVGL 当前 buf 的指针

### 4.2 `lv_port_disp_flush_ready` — 暴露给 ISR

```c
void lv_port_disp_flush_ready(void)
{
    dma_busy = false;                // 先解锁硬件
    lv_display_flush_ready(my_disp); // 再释放 LVGL buffer
}
```

**为什么顺序重要：** 先解锁 `dma_busy`，这样如果需要立刻发下一块，`disp_flush` 可以获取锁。再调 `lv_display_flush_ready` 让 LVGL 把当前 buf 标记为空闲。

### 4.3 `ST7789_FlushArea_DMA` — 异步版窗口写入

```c
void ST7789_FlushArea_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          const uint8_t* data)
{
    // [1] CASET + RASET 设定写入窗口（阻塞发送——命令不可异步）
    // [2] CS↓, DC↓, 发 RAMWR(0x2C), DC↑
    // [3] HAL_SPI_Transmit_DMA(data, w*h*2) ← 异步，函数立即返回
    // [4] CS 保持低，等 DMA 中断回调里拉高
}
```

**为什么命令用阻塞发送：** CASET、RASET、RAMWR 命令每个只有几个字节，阻塞发送开销极小（~1μs），用 DMA 反而增加复杂度。真正的传输大头是像素数据（几十KB），异步才有价值。

### 4.4 `HAL_SPI_TxCpltCallback` — DMA 完成回调

```c
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (LCD_SPI == hspi) {             // 只处理 SPI2
        ST7789_FlushComplete();        // CS↑ 结束本次写屏
        lv_port_disp_flush_ready();    // 释放 LVGL buffer
    }
}
```

**为什么 CS 在中断里拉高：** 如果 DMA 还在发数据就拉 CS，ST7789 认为传输结束，后半段像素丢失，画面撕裂。必须等 DMA 完全发完（TC 中断）才拉 CS。

### 4.5 `lv_display_set_buffers` — 注册双缓冲

```c
lv_display_set_buffers(my_disp, buf_1, buf_2, sizeof(buf_1),
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
//                          ^^^^^  ^^^^^
//                          第一块  第二块（传 NULL = 单缓冲）
```

LVGL 内部自动管理两块缓冲的轮换。开发者不需要写任何切换逻辑。

---

## 五、统一版图

```
┌─────────────────────────────────────────────────────────┐
│                      FreeRTOS 调度                       │
│  ┌─────────────┐  ┌──────────────┐                      │
│  │ defaultTask  │  │  lvglTask    │ ← 优先级 Normal      │
│  │ (空循环)     │  │  UI 主循环    │                      │
│  └─────────────┘  └──────┬───────┘                      │
│                          │ 5ms 周期                      │
│                    ┌─────▼──────┐                        │
│                    │ lv_timer   │                        │
│                    │ _handler() │ LVGL 内核              │
│                    └─────┬──────┘                        │
│                          │ 检测脏区、渲染                 │
│                    ┌─────▼──────┐                        │
│                    │ disp_flush │ 异步 DMA               │
│                    └─────┬──────┘                        │
│                          │ 启动 DMA，返回                │
│                    ┌─────▼──────┐                        │
│                    │ app_loop() │ 页面刷新、手势          │
│                    └─────┬──────┘                        │
│                          │                              │
│  ┌───────────────────────▼──────────────────────────┐   │
│  │              DMA1_Stream4 (后台)                  │   │
│  │  [SRAM buf] ────→ [SPI2->DR] ────→ [ST7789]     │   │
│  └───────────────────────┬──────────────────────────┘   │
│                          │ 传输完成中断                   │
│  ┌───────────────────────▼──────────────────────────┐   │
│  │         HAL_SPI_TxCpltCallback (ISR)             │   │
│  │         → CS↑ → dma_busy=0 → flush_ready        │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## 六、面试可能会问

### Q1: DMA 和中断有什么区别？

**答（简短）：** 中断是"CPU 替外设做事"——外设发请求，CPU 暂停当前任务去处理；DMA 是"外设自己做事"——DMA 控制器直接读写内存，CPU 全程不参与。

**答（详细）：** 没有 DMA 时，每发一个字节到 SPI，CPU 要执行：读内存 → 写 SPI 数据寄存器 → 等 TXE 标志 → 循环。100KB 数据可能要上百万个指令。DMA 把同一件事外包给 DMA 控制器——CPU 只要说"从这个地址取 100KB 发到 SPI2"，全程 CPU 可以做渲染、计算等。DMA 干完触发一个中断通知 CPU 收工。

关键指标：SPI 阻塞方式 CPU 利用率 ~100%；DMA 方式 CPU 利用率降至几个百分点（只用在命令发送和中断处理）。

### Q2: 双缓冲和单缓冲的区别是什么？为什么要用双缓冲？

**答（简短）：** 单缓冲时，生产者（LVGL）必须等消费者（DMA）用完才能继续生产；双缓冲时，生产者和消费者各用一块，并行工作。

**答（详细）：** 这其实就是计算机图形学里"前缓冲/后缓冲"概念。LCD 正在显示 buf_1 时，GPU 往 buf_2 画下一帧。这里 LVGL 是 GPU，DMA 是"屏幕消化通道"。单缓冲：画完→发送→等→画完→发送→等...（串行）。双缓冲：buf_1 在发送时 buf_2 可以被画（并行）。

现实场景类比：洗碗。单缓冲 = 一个碗，你洗一个碗 → 别人拿去擦 → 你等擦完才能洗下一个。双缓冲 = 两个碗，你洗你的，他擦他的，两人同时干活。

### Q3: `volatile` 关键字在 DMA 场景下有什么用？

**答：** `dma_busy` 这个变量会被两个执行上下文访问——`disp_flush`（LVGL 任务）和 `HAL_SPI_TxCpltCallback`（DMA 中断）。编译器如果不知道这个变量可能被中断修改，会在优化时把它的值缓存到寄存器里，导致忙等循环变成死循环（永远读寄存器里的旧值）。`volatile` 通知编译器"每次都要从内存读，别缓存"。

代码示例：
```c
static volatile bool dma_busy = false;  // volatile 必不可少！
// ...
while (dma_busy) {}  // 没有 volatile → 编译器可能优化成 if(dma_busy) while(1);
```

### Q4: DMA Normal 模式和 Circular 模式有什么区别？

**答：**

| | Normal | Circular |
|---|---|---|
| 行为 | 发完指定字节数后停止 | 发完自动从头开始 |
| 中断 | 每轮发完中断一次 | 每轮发完中断一次但不停止 |
| 用途 | 离散传输（一帧数据） | 连续流（音频、ADC 采集） |
| CPU 参与 | 每帧手动重启 | 只需配置一次 |

本项目用 Normal：LVGL 每次给一块，DMA 发一块就停，等下一块来再手动启动。Circular 适合音频 DAC——缓冲区循环发不停。

### Q5: SPI 用 DMA 时，为什么要加 `dma_busy` 互斥锁？

**答：** 两个保护层面，缺一不可：

**内存层面**（LVGL 保证）：LVGL 两冲之间的 `flush_ready` 机制确保同一块 buf 不会被同时读写。这是软件层的保护。

**外设层面**（dma_busy 保证）：SPI 是共享外设，同一时间 DMA 只能在上面跑一个传输。如果上一个 DMA 还没发完，下一个 `disp_flush` 又调 `HAL_SPI_Transmit_DMA`，HAL 返回 `HAL_BUSY` 直接丢数据。`dma_busy` 在启动 DMA 前置位，ISR 里清位，是一种基于忙等的互斥锁。

**追问：为什么不用 FreeRTOS 信号量而是忙等？**

忙等时间是确定的——最坏情况是上一次的全缓冲（20行×240×2B @25MHz ≈ 1.5ms）。在 ISR 中 Give 信号量、任务中 Take 信号量的开销 ≈10-20μs，比忙等小，但增加复杂度和栈用量。对于 <2ms 的等待时间，忙等是可接受的。

### Q6: 双缓冲中 LVGL 怎么知道该用哪块？

**答：** LVGL 内部有一个简单的**空闲/占用标记**。每次调用 `disp_flush` 时，LVGL 记录发给你的这块 buf 地址。之后在渲染下一块前，检查哪块 buf 没被占用（即还没有对应的 `flush_ready`）。开发者完全不需要管理轮换，LVGL 内核自动处理。

```c
// LVGL 内部逻辑（简化版，不在你的代码里）
if (buf_1_busy) {
    render_to(buf_2);
} else {
    render_to(buf_1);
}
```

`buf_1_busy` 在 `disp_flush(buf_1)` 时被设为 true，在 `lv_display_flush_ready(disp)` 时被设为 false。

### Q7: 缓冲区能大到覆盖整帧吗？为什么不？

**答：** 算一下就清楚。一帧 240×280×2字节（RGB565）= 131,040 字节 ≈ 128KB。STM32F411CE 总共只有 128KB SRAM，还要跑 FreeRTOS、LVGL、各种驱动。一帧也存不下。

PARTIAL 模式就是为小内存嵌入式系统设计的——少量缓冲、多次 flush。

---

## 七、陷阱与教训

1. **CubeMX 重新生成会覆盖代码** — 所有自定义代码必须写在 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 之间，否则下次生成就没了。本项目吃了这个亏：`lv_tick_inc(1)` 写在标记外，被 CubeMX 覆盖导致整个手势系统瘫痪。

2. **DMA 中断优先级不能太低** — 设为 5，与 FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY` 同级。优先级太高（数值太小）可能抢断关键时序；太低（数值太大）会被 FreeRTOS 临界区屏蔽。

3. **`volatile` 不要漏** — 任何被 ISR 和普通代码共享的变量必须加 `volatile`，否则编译器优化后行为异常。

4. **先跑通再优化** — `buf_DMA` 中间缓冲最初加了作为安全措施，验证 DMA 链路正常工作后删除。过早优化是万恶之源，先上防弹衣，活下来再脱。

5. **TE 信号不能假设** — ST7789 模组的 BUSY 脚不一定是 TE 信号。不同厂家命名习惯不同，必须查资料或用示波器验证。
