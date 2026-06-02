/**
 * @file    ipc_defs.h
 * @brief   所有 IPC (任务间通信) 对象的统一定义
 *
 * @details 设计文档规定只有 4 种合法的任务间通信方式:
 *            1. 共享内存 + TaskNotify  (高频, 可丢失)
 *            2. 消息队列               (低频, 不可丢失)
 *            3. Event Group            (多任务同步)
 *            4. Software Timer 回调    (定时触发)
 *
 *          本文件定义方式 2 和方式 3 所需的:
 *            - 队列句柄     (extern 声明, 实物在 ipc_defs.c)
 *            - 事件组句柄   (extern 声明, 实物在 ipc_defs.c)
 *            - 消息体结构   (定义了队列里传递的"包裹"长什么样)
 *            - 事件位宏     (定义了事件组的每一盏"灯"代表什么)
 *
 *          (方式 1 的共享内存定义在 shared_memory.h)
 *
 *          命名规则:
 *            g_q_xxx       → 全局队列       (Queue)
 *            g_eg_xxx      → 全局事件组     (Event Group)
 *            xxx_msg_t     → 消息体类型     (Message)
 *            xxx_evt_t     → 事件体类型     (Event)
 */

#ifndef IPC_DEFS_H
#define IPC_DEFS_H

#include "FreeRTOS.h"
#include "queue.h"          /* QueueHandle_t, xQueueCreate / xQueueSend */
#include "event_groups.h"   /* EventGroupHandle_t, xEventGroupCreate 等 */

/* ═══════════════════════════════════════════════════════════════
 *  消息队列 (Message Queue) — 低频不可丢失的数据
 *
 *  队列像快递柜:
 *    - 有深度 (同时最多存几条消息)
 *    - 有大小 (每条消息最大多少字节)
 *    - 生产者塞进去, 消费者取出来
 *    - 满了可以覆盖最老的 (Ring Buffer 模式), 或者阻塞等待
 *
 *  注意: 此处只是声明 (extern), 真正的创建 (xQueueCreate)
 *        在 freertos.c 的 MX_FREERTOS_Init() 中进行。
 *        句柄初始值为 NULL (ipc_defs.c), 创建后指向有效对象。
 * ═══════════════════════════════════════════════════════════════ */

/* BtTask → LvglTask: BLE 消息 (通知/同步/OTA/AT响应)
 * 深度 4, 元素大小 sizeof(ble_msg_t) ≈ 260 字节 */
extern QueueHandle_t g_q_ble_msg;

/* I2CSensTask / BtTask → SaveTask: 存储请求 (历史数据/配置/OTA固件块)
 * 深度 4, 元素大小 sizeof(save_req_t) */
extern QueueHandle_t g_q_save_req;

/* PowerTask → LvglTask: 电源事件 (低电量/充电/亮度变化)
 * 深度 4, 元素大小 sizeof(power_evt_t) */
extern QueueHandle_t g_q_power_event;

/* ═══════════════════════════════════════════════════════════════
 *  Event Groups (事件组) — 多任务同步
 *
 *  事件组像一块有 24 盏信号灯的控制板 (24-bit bitmask):
 *    - xEventGroupSetBits()   → 点亮指定的灯
 *    - xEventGroupWaitBits()  → 等待某些灯全部点亮
 *    - xEventGroupGetBits()   → 看看现在亮着哪些灯
 *
 *  比普通变量好的地方:
 *    - 等待期间不占 CPU (任务被 FreeRTOS 挂起, 灯亮才唤醒)
 *    - 内部有临界区保护, 不会"读-改-写"被打断
 * ═══════════════════════════════════════════════════════════════ */

/* 休眠协调: PowerTask 发出"准备休眠" → 各任务点灯回应"就绪" → 进入 STOP */
extern EventGroupHandle_t g_eg_sleep;
/* 系统状态标志: SYS_READY / SYS_OTA_MODE / SYS_SAFE_MODE */
extern EventGroupHandle_t g_eg_sys_state;

/* ── g_eg_sleep 事件位 (休眠握手协议) ──
 *
 *  流程:
 *    1. PowerTask 调用 xEventGroupSetBits(g_eg_sleep, EVT_PREPARE_SLEEP)
 *    2. 各任务收到后各自收拾 (关传感器/刷缓存/停渲染)
 *    3. 各自调用 xEventGroupSetBits(g_eg_sleep, ACK_xxx_READY)
 *    4. PowerTask 调用 xEventGroupWaitBits 等所有 ACK 亮 → 进 STOP
 *    5. 唤醒后 PowerTask 设 EVT_WAKEUP → 各任务恢复外设             */
#define EVT_PREPARE_SLEEP   (1 << 0)  /* bit0: PowerTask 发起休眠请求   */
#define ACK_LVGL_READY      (1 << 1)  /* bit1: LvglTask   就绪         */
#define ACK_I2C_READY       (1 << 2)  /* bit2: I2CSensTask 就绪        */
#define ACK_BT_READY        (1 << 3)  /* bit3: BtTask      就绪        */
#define ACK_SAVE_READY      (1 << 4)  /* bit4: SaveTask    就绪        */
#define ACK_DIAG_READY      (1 << 5)  /* bit5: DiagTask    就绪        */
#define EVT_WAKEUP          (1 << 6)  /* bit6: 系统从 STOP 模式醒来     */

/* ── g_eg_sys_state 事件位 (系统运行模式) ──
 *
 *  和 g_eg_sleep 不同, 这些灯是长期亮着的, 表示"当前处于什么模式"。
 *  任何任务都可以随时查看, 比如:
 *    LvglTask:  "SYS_SAFE_MODE 亮着吗? 亮了我就显示传感器故障警告"
 *    BtTask:    "SYS_OTA_MODE 亮着吗? 亮了我就只收 OTA 数据, 不收普通消息" */
#define SYS_READY           (1 << 0)  /* bit0: 所有任务初始化完毕       */
#define SYS_OTA_MODE        (1 << 1)  /* bit1: 正在固件升级中           */
#define SYS_SAFE_MODE       (1 << 2)  /* bit2: 安全模式 (传感器故障降级) */

/* ═══════════════════════════════════════════════════════════════
 *  BLE 消息体 — 在 g_q_ble_msg 队列中传递
 *
 *  队列深度 4 的原因:
 *    手机短时间内发了 4 条通知 → 第 5 条才覆盖最老的。
 *    正常场景下 4 条凑不满, 所以基本不会丢消息。
 *    payload[256] 固定 256 字节: 嵌入式不能用 malloc,
 *    编译时确定大小, 避免内存碎片。
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {
    BLE_MSG_NOTIFICATION = 0,   /* 手机来通知 (微信/短信/来电)           */
    BLE_MSG_SYNC_DATA,          /* 手机请求同步 (步数/心率历史)          */
    BLE_MSG_TIME_SYNC,          /* 手机校准手表时间                      */
    BLE_MSG_OTA_START,          /* 固件升级开始                          */
    BLE_MSG_OTA_CHUNK,          /* 固件升级数据块                        */
    BLE_MSG_OTA_END,            /* 固件升级完成                          */
    BLE_MSG_CMD_RESP,           /* AT 指令响应                           */
} ble_msg_type_t;

typedef struct {
    ble_msg_type_t type;        /* 这条消息是什么类型                    */
    uint8_t  payload[256];      /* 消息内容 (AT数据 / 通知文本)         */
    uint16_t payload_len;       /* payload 实际有效字节数               */
} ble_msg_t;

/* ═══════════════════════════════════════════════════════════════
 *  存储请求体 — 在 g_q_save_req 队列中传递
 *
 *  SaveTask 独占 SPI2 → W25Q64 Flash。
 *  其他任务不能直接写 Flash, 必须通过这个队列提交请求。
 *  这叫"总线独占"——SPI2 只有 SaveTask 碰, 永远不需要互斥锁。
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {
    SAVE_REQ_HISTORY = 0,       /* 保存运动/心率历史数据                 */
    SAVE_REQ_CONFIG,            /* 保存系统配置 (亮度/表盘/闹钟)         */
    SAVE_REQ_OTA_CHUNK,         /* 保存 OTA 固件数据块到 Flash          */
} save_req_type_t;

typedef struct {
    save_req_type_t type;       /* 存什么类型的数据                      */
    uint32_t flash_addr;        /* 写到 Flash 的哪个地址                */
    const uint8_t *data;        /* 要写的数据指针 (指向原始数据)         */
    uint16_t data_len;          /* 数据长度                              */
    uint8_t  callback_needed;   /* 写完后要不要用 TaskNotify 回复        */
} save_req_t;

/* ═══════════════════════════════════════════════════════════════
 *  电源事件体 — 在 g_q_power_event 队列中传递
 *
 *  PowerTask 监控电池电压和充电状态, 变化时推送事件给 LvglTask,
 *  LvglTask 据此更新状态栏电量图标 / 弹出低电量提醒。
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {
    PWR_EVT_LOW_BATTERY = 0,    /* 低电量 (< 10%)                       */
    PWR_EVT_CHARGING,           /* 充电器插入                            */
    PWR_EVT_UNPLUGGED,          /* 充电器拔出                            */
    PWR_EVT_BRIGHTNESS_CHG,     /* 用户调整了亮度                         */
} power_evt_type_t;

typedef struct {
    power_evt_type_t type;      /* 事件类型                              */
    uint8_t value;              /* 附带数值 (电量百分比 / 亮度值)        */
} power_evt_t;

#endif
