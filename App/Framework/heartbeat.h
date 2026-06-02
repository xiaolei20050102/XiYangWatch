#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdint.h>

#define MAX_MONITORED_TASKS  6

typedef enum {
    TASK_LVGL = 0,
    TASK_I2CSENS,
    TASK_BT,
    TASK_SAVE,
    TASK_POWER,
    TASK_DIAG,
} task_id_t;

/* ── 任务侧 API ── */
void heartbeat_register(task_id_t id, uint32_t timeout_ms);
void heartbeat_tick(task_id_t id);

/* ── DiagTask 侧 API ── */
void heartbeat_monitor_all(void);

#endif
