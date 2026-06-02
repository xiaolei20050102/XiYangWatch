/**
 * @file    log_port.h
 * @brief   全项目统一日志接口 — 可编译期开关
 *
 * @details 开发阶段 LOG_ENABLE=1, 发布改 0, 所有日志调用编译后完全消失。
 *          换日志后端只需改 log_port.c 里的实现。
 */

#ifndef LOG_PORT_H
#define LOG_PORT_H

/* ── 日志总开关: 1=输出日志, 0=编译期消除 ── */
#define LOG_ENABLE  1

#if LOG_ENABLE
    void log_init(void);
    void log_printf(const char *fmt, ...);
#else
    #define log_init()      ((void)0)
    #define log_printf(...) ((void)0)
#endif

#endif
