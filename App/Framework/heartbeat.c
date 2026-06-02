/**
 * @file    heartbeat.c
 * @brief   任务心跳监控实现
 *
 * @details 每个 FreeRTOS 任务在主循环中定时"报到"（调用 heartbeat_tick）。
 *          DiagTask 每秒巡检一次（调用 heartbeat_monitor_all），
 *          发现某个任务连续 3 次超时未报到 → 判定为卡死 → 软复位。
 *
 *          这是设计文档第九节故障恢复策略中的 L4 级别故障处理。
 *
 *  使用示例:
 *    // 任务初始化时注册（只调一次）
 *    heartbeat_register(TASK_I2CSENS, 200);  // 200ms 内必须报到
 *
 *    // 任务主循环中报到（每周期调一次）
 *    while (1) {
 *        干活...
 *        heartbeat_tick(TASK_I2CSENS);  // "我还活着"
 *        vTaskDelay(5);
 *    }
 *
 *    // DiagTask 中巡检（每秒调一次）
 *    heartbeat_monitor_all();
 */

#include "heartbeat.h"
#include "stm32f4xx_hal.h"

/* ═══════════════════════════════════════════════════════════════
 *  心跳记录表 — 每个被监控的任务占一行
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t last_tick;    /* 最后一次报到的时间戳 (HAL_GetTick 返回值, 单位 ms) */
    uint32_t timeout_ms;   /* 允许的最大沉默时间, 超过这个时间没报到就是超时       */
    uint8_t  registered;   /* 1=这个位置有人注册了, 0=空位                        */
    uint8_t  dead_count;   /* 连续超时次数, 用于防抖——偶尔一次不算死              */
} heartbeat_entry_t;

/* 全局心跳登记表, 共 MAX_MONITORED_TASKS 行, 初始全为 0 */
static heartbeat_entry_t g_heartbeats[MAX_MONITORED_TASKS];

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_register — 任务初始化时注册
 *
 *  每个任务启动后调用一次, 告诉心跳系统:
 *  "我是任务 id, 我会每 timeout_ms 毫秒报到一次, 如果超时没来, 就算我挂了。"
 *
 *  参数:
 *    id         : 任务编号 (TASK_LVGL / TASK_I2CSENS / ...)
 *    timeout_ms : 超时阈值, 单位毫秒
 *                 LvglTask→1000, I2CSensTask→200, BtTask→5000 等
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_register(task_id_t id, uint32_t timeout_ms)
{
    /* 越界检查: 给了一个不存在的任务编号, 静默忽略 */
    if (id >= MAX_MONITORED_TASKS) return;

    g_heartbeats[id].last_tick   = HAL_GetTick();  /* 记录注册时刻, 避免一注册就超时 */
    g_heartbeats[id].timeout_ms  = timeout_ms;     /* 设定这个任务的"容忍度"          */
    g_heartbeats[id].registered  = 1;              /* 标记为已注册                    */
    g_heartbeats[id].dead_count  = 0;              /* 从零开始计数                    */
}

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_tick — 任务报到
 *
 *  任务在主循环 while(1) 末尾调用, 相当于喊一声"我还活着"。
 *  每调一次, 就把自己的 last_tick 刷新为当前时间。
 *
 *  开销极小: 只有一次数组索引 + 一次函数调用 + 一次赋值。
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_tick(task_id_t id)
{
    if (id >= MAX_MONITORED_TASKS) return;

    /* 只做一件事: 记录"我这一刻还活着" */
    g_heartbeats[id].last_tick = HAL_GetTick();
}

/* ═══════════════════════════════════════════════════════════════
 *  heartbeat_monitor_all — 巡检全部任务 (由 DiagTask 每秒调用)
 *
 *  遍历所有已注册任务的心跳记录:
 *    - now - last_tick > timeout_ms → 超时, dead_count++
 *    - dead_count >= 3             → 确认卡死, 软复位
 *    - 没超时                      → dead_count 清零 (恢复健康)
 *
 *  为什么需要连续 3 次才复位?
 *    I2C 通信偶尔会有毛刺, 一次失败下一秒可能就恢复了。
 *    如果一超时就复位 → 手表一天要重启几十次。
 *    3 次连续超时 = 连续 3 次巡检都没恢复 = 确认真的死了。
 *
 *  注意: HAL_GetTick 在 STOP 模式下会暂停, 但不影响超时判断
 *        (因为休眠期间这个函数根本不会被调用)
 * ═══════════════════════════════════════════════════════════════ */
void heartbeat_monitor_all(void)
{
    uint32_t now = HAL_GetTick();  /* 取一次当前时间, 避免循环内重复调 */

    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {

        /* 这个位置还没人注册, 跳过 */
        if (!g_heartbeats[i].registered) continue;

        /* ── 超时判断 ──
         * now - last_tick > timeout_ms 的意思是:
         *   "距离上次报到已经过去了 X 毫秒, 超过了允许的沉默时间"
         * 注意: HAL_GetTick 是 uint32_t, 做减法会自动处理翻转
         *       (比如 now=100, last_tick=0xFFFFFFFF, 差还是正确的) */
        if (now - g_heartbeats[i].last_tick > g_heartbeats[i].timeout_ms) {

            g_heartbeats[i].dead_count++;  /* 记一次超时 */

            /* 连续 3 次巡检都超时 → 确认任务真的死了 → 软复位 */
            if (g_heartbeats[i].dead_count >= 3) {
                /*
                 * HAL_Delay(50) 的目的:
                 *   如果 DiagTask 在复位前通过 UART2 发了诊断日志,
                 *   这 50ms 留给串口把缓冲区最后的数据发完,
                 *   这样 RTT 查看器 / 串口终端上能看到完整的死亡报告。
                 */
                HAL_Delay(50);

                /* 软复位: 等同于按了复位按钮, 程序从头开始执行
                 * 区别是 RCC 复位标志会被设为软件复位, 而不是上电复位 */
                NVIC_SystemReset();
            }

        } else {
            /* 没超时 → 这个任务健康, 把连续死亡计数清零
             * 这样偶尔一次毛刺不会累积, 只有连续失败才会触发复位 */
            g_heartbeats[i].dead_count = 0;
        }
    }
}
