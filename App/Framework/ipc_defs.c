/**
 * @file    ipc_defs.c
 * @brief   全局 IPC 对象实例定义 — CMSIS-RTOS v2
 *
 * @details 所有在 ipc_defs.h 中 extern 声明的队列和事件标志句柄,
 *          实物都在这里。初始值全部为 NULL。
 *
 *          真正的创建 (osMessageQueueNew / osEventFlagsNew)
 *          在 Core/Src/freertos.c 的 MX_FREERTOS_Init() 中进行,
 *          时机: osKernelInitialize → 创建 IPC → 创建任务 → osKernelStart
 */

#include "ipc_defs.h"

/* ═══ 消息队列实例 ═══ */
osMessageQueueId_t g_q_ble_msg     = NULL;   /* BLE 消息队列 (BtTask → LvglTask)     */
osMessageQueueId_t g_q_save_req    = NULL;   /* 存储请求队列 (→ SaveTask)             */
osMessageQueueId_t g_q_power_event = NULL;   /* 电源事件队列 (PowerTask → LvglTask)   */

/* ═══ 事件标志实例 ═══ */
osEventFlagsId_t g_ef_sleep     = NULL;      /* 休眠协调 (PowerTask → 全员)            */
osEventFlagsId_t g_ef_sys_state = NULL;      /* 系统状态标志                            */
