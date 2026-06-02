#ifndef DIAG_TASK_H
#define DIAG_TASK_H

/* DiagTask — 系统诊断 & 双层看门狗管理
 *
 * 优先级: 最低 (osPriorityLow)
 * 栈大小: 1KB (64 words)
 * 周期:   1s
 *
 * 职责:
 *   ① 读复位原因 → UART2 输出
 *   ② 每秒扫描任务心跳 → 超时 3 次 → 软复位
 *   ③ 每秒喂硬件 IWDG → 防止 CPU 锁死
 *   ④ 统计剩余内存 → UART2 输出
 */

void DiagTask(void *pvParameters);

#endif
