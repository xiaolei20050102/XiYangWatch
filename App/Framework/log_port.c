/**
 * @file    log_port.c
 * @brief   日志底层实现 — SEGGER_RTT
 */

#include "log_port.h"

#if LOG_ENABLE

#include "SEGGER_RTT.h"

void log_init(void)
{
    SEGGER_RTT_Init();
}

void log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    SEGGER_RTT_vprintf(0, fmt, &args);
    va_end(args);
}

#endif /* LOG_ENABLE */
