#include "diag_task.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"           /* vTaskDelay, pdMS_TO_TICKS */
#include "stm32f4xx_hal.h"  /* HAL_GetTick, HAL_IWDG, HAL_UART, __HAL_RCC */
#include "heartbeat.h"      /* heartbeat_register / heartbeat_tick / heartbeat_monitor_all */
#include "log_port.h"       /* log_printf — 全项目统一日志 */

extern osThreadId_t lvglTaskHandle;
extern osThreadId_t powerTaskHandle;

/* IWDG 句柄 + 初始化 — 放在 DiagTask 内部，确保狗在任务跑起来之后才开始倒数 */
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
    /* ═══ 阶段 1: 读出复位原因 ═══
     * RCC_CSR 寄存器记录了上次复位的原因:
     *   RCC_FLAG_IWDGRST  = 1 → 被硬件看门狗咬的
     *   RCC_FLAG_SFTRSTF  = 1 → 软件复位 (NVIC_SystemReset)
     *   RCC_FLAG_PORRSTF  = 1 → 上电复位 (正常开机)
     * 读完后必须清除, 否则下次复位时新旧标志混在一起分不清 */
    uint32_t rst_flag = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST);
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* ═══ 阶段 2: 打印启动横幅 ═══ */
    log_printf("\r\n=== XiYangWatch v1.0 ===\r\n");
    log_printf(rst_flag ? "Reset: IWDG\r\n"
                         : "Reset: POR \r\n");

    /* ═══ 阶段 3: 注册心跳 (DiagTask 自己也要报到) ═══
     * 超时 3000ms — 比其他任务宽松, 因为它在最低优先级,
     * 高优先级任务繁忙时可能长时间抢不到 CPU */
    heartbeat_register(TASK_DIAG, 3000);
    heartbeat_tick(TASK_DIAG);

    /* ═══ 阶段 4: 启动 IWDG — 必须在这里而不是 main.c, 因为:
     *   main.c → 调度器启动 → LvglTask 先初始化 (可能几百ms)
     *   → DiagTask 终于跑起来 → 现在才开始喂狗
     *   如果狗在 main.c 就启动, LvglTask 初始化期间可能触发误复位 ═══ */
    MX_IWDG_Init();

    /* ═══ 阶段 5: 主循环 (每秒巡检) ═══ */
    for (;;)
    {
        /* ① 扫描所有任务心跳: 谁超过 timeout_ms 没报到就记一次,
         *    连续 3 次 → 软复位 (NVIC_SystemReset) */
        heartbeat_monitor_all();

        log_printf("[DIAG] HeapFree:%u LvglStackHWM:%u\r\n",
        xPortGetFreeHeapSize(),
        uxTaskGetStackHighWaterMark(lvglTaskHandle));

        log_printf("[DIAG] HeapFree:%u PowerStackHWM:%u\r\n",
        xPortGetFreeHeapSize(),
        uxTaskGetStackHighWaterMark(powerTaskHandle));        

        /* ② 喂硬件看门狗: IWDG 超时 2s, 每 1s 喂一次,
         *    DiagTask 一卡 → 没人喂狗 → IWDG 硬复位拯救系统 */
        HAL_IWDG_Refresh(&s_hiwdg);

        /* ③ 自身报到: DiagTask 还活着 */
        heartbeat_tick(TASK_DIAG);

        /* ④ 阻塞 1 秒 — 必须有阻塞点, 不能空转 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
