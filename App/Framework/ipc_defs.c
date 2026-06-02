/**
 * @file    ipc_defs.c
 * @brief   全局 IPC 对象实例定义
 *
 * @details 所有在 ipc_defs.h 中 extern 声明的队列和事件组句柄,
 *          实物都在这里。初始值全部为 NULL。
 *
 *          真正的创建 (xQueueCreate / xEventGroupCreate)
 *          在 Core/Src/freertos.c 的 MX_FREERTOS_Init() 中进行,
 *          时机: osKernelInitialize 之后 → 创建 IPC → 创建任务 → osKernelStart
 *
 *          这个顺序保证了: 任务一启动就能用队列, 不需要自己检查"队列创建了没"。
 */

#include "ipc_defs.h"

/* ═══════════════════════════════════════════════════════════════
 *  消息队列实例
 *  创建时机: MX_FREERTOS_Init() (内核初始化后, 任务启动前)
 *  创建方式: xQueueCreate(队列深度, 每个元素大小)
 * ═══════════════════════════════════════════════════════════════ */

QueueHandle_t g_q_ble_msg     = NULL;   /* BLE 消息队列       (BtTask → LvglTask)      */
QueueHandle_t g_q_save_req    = NULL;   /* 存储请求队列       (→ SaveTask)               */
QueueHandle_t g_q_power_event = NULL;   /* 电源事件队列       (PowerTask → LvglTask)     */

/* ═══════════════════════════════════════════════════════════════
 *  事件组实例
 *  创建时机: 同上
 *  创建方式: xEventGroupCreate()
 * ═══════════════════════════════════════════════════════════════ */

EventGroupHandle_t g_eg_sleep     = NULL;   /* 休眠协调 (PowerTask → 全员)     */
EventGroupHandle_t g_eg_sys_state = NULL;   /* 系统状态标志                    */
