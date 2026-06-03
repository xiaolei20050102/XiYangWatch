/**
 * @file    diag_task.c
 * @brief   DiagTask — 系统诊断 + 双层看门狗管理 — CMSIS-RTOS v2
 */

#include "diag_task.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "heartbeat.h"
#include "log_port.h"

/* 外部任务句柄 (freertos.c 中 osThreadNew 创建时返回) */
extern osThreadId_t lvglTaskHandle;
extern osThreadId_t powerTaskHandle;

/* IWDG 句柄 + 初始化 */
static IWDG_HandleTypeDef s_hiwdg;

static void MX_IWDG_Init(void)
{
    s_hiwdg.Instance       = IWDG;
    s_hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    s_hiwdg.Init.Reload    = 2000;
    HAL_IWDG_Init(&s_hiwdg);
}

void DiagTask(void *pvParameters)
{
    /* ① 读复位原因 */
    uint32_t rst_flag = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST);
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* ② 打印启动横幅 */
    log_printf("\r\n=== XiYangWatch v1.0 ===\r\n");
    log_printf(rst_flag ? "Reset: IWDG\r\n" : "Reset: POR \r\n");

    /* ③ 注册心跳 */
    heartbeat_register(TASK_DIAG, 3000);
    heartbeat_tick(TASK_DIAG);

    /* ④ 启动 IWDG (在 DiagTask 内部启动, 避免 LvglTask 初始化期间误复位) */
    MX_IWDG_Init();

    /* ⑤ 主循环: 每秒巡检 */
    for (;;)
    {
        heartbeat_monitor_all();

        log_printf("[DIAG] HeapFree:%u LvglStackHWM:%u\r\n",
                   xPortGetFreeHeapSize(),
                   osThreadGetStackSpace(lvglTaskHandle));

        log_printf("[DIAG] HeapFree:%u PowerStackHWM:%u\r\n",
                   xPortGetFreeHeapSize(),
                   osThreadGetStackSpace(powerTaskHandle));

        HAL_IWDG_Refresh(&s_hiwdg);

        heartbeat_tick(TASK_DIAG);
        osDelay(1000);
    }
}
