/**
 * @file    shared_memory.c
 * @brief   全局共享内存 g_shm 的唯一实例
 *
 * @details 整个系统只有这一个 g_shm 实例。
 *          {0} 初始化保证所有字段从零开始:
 *            - touch.pressed = 0  (初始无触摸)
 *            - sens_health 全部 = SENS_OK (0)
 *            - 所有 last_update_tick = 0
 *            - 等等
 *
 *          这个文件只有一行有效代码, 但它是最重要的文件之一:
 *          g_shm 是所有任务间数据交换的中枢。
 */

#include "shared_memory.h"

/* volatile:
 *   告诉编译器"这个变量会被其他任务或中断修改，不要缓存"。
 *   Cortex-M4 上每次读 g_shm.xxx 都会从内存重新读, 不会用寄存器里的旧值。
 *
 * {0}:
 *   C 语言的零初始化语法。等效于把所有字节设为 0。
 *   在嵌入式里, .bss 段 (全零变量) 不占 Flash 空间——启动时由 crt0 清零。 */
volatile shared_mem_t g_shm = {0};
