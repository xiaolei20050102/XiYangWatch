/**
 * @file    w25q64_fs.c
 * @brief   W25Q64 LVGL 文件系统驱动实现
 * @note    用固定查找表将文件路径映射到 W25Q64 地址
 */

#include "w25q64_fs.h"
#include "w25q64.h"
#include "lvgl.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * 文件查找表 —— 添加新文件在此处注册
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    const char *path;
    uint32_t    addr;
    uint32_t    size;
} file_entry_t;

#define WP_SIZE (12U + 240U * 280U * 2U)  /* header(12) + RGB565 pixels(134400) */
#define WP_ADDR 0x00000000U

static const file_entry_t g_files[] = {
    { "wallpaper.bin", WP_ADDR, WP_SIZE },
};
#define FILE_COUNT (sizeof(g_files) / sizeof(g_files[0]))

/* ═══════════════════════════════════════════════════════════════
 * 文件句柄 —— malloc 分配，存入 file_d
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    const file_entry_t *entry;
    uint32_t            pos;
} file_handle_t;

/* ═══════════════════════════════════════════════════════════════
 * LVGL 文件系统回调
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  打开文件，查表找到对应 W25Q64 地址
 * @retval 非 NULL — 文件句柄 / NULL — 打开失败
 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);

    if (mode != LV_FS_MODE_RD) return NULL;

    /* 跳过 "/W:" 或 "/" 前缀（LVGL 已将驱动盘符剥离） */
    const char *name = path;
    if (name[0] == '/') name++;

    /* 查表 */
    const file_entry_t *entry = NULL;
    for (uint32_t i = 0; i < FILE_COUNT; i++) {
        if (strcmp(name, g_files[i].path) == 0) {
            entry = &g_files[i];
            break;
        }
    }
    if (!entry) return NULL;

    /* 分配句柄 */
    file_handle_t *h = lv_malloc(sizeof(file_handle_t));
    if (!h) return NULL;

    h->entry = entry;
    h->pos   = 0;

    return h;
}

/**
 * @brief  读取文件数据
 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    LV_UNUSED(drv);

    file_handle_t *h = (file_handle_t *)file_p;
    if (!h) return LV_FS_RES_INV_PARAM;

    /* 裁剪到文件末尾 */
    uint32_t remain = h->entry->size - h->pos;
    if (btr > remain) btr = remain;

    if (btr == 0) {
        if (br) *br = 0;
        return LV_FS_RES_OK;
    }

    if (!w25q64_read(h->entry->addr + h->pos, (uint8_t *)buf, btr)) {
        return LV_FS_RES_HW_ERR;
    }

    h->pos += btr;
    if (br) *br = btr;
    return LV_FS_RES_OK;
}

/**
 * @brief  设置读写位置
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    LV_UNUSED(drv);

    file_handle_t *h = (file_handle_t *)file_p;
    if (!h) return LV_FS_RES_INV_PARAM;

    int32_t new_pos;
    switch (whence) {
    case LV_FS_SEEK_SET: new_pos = (int32_t)pos;                        break;
    case LV_FS_SEEK_CUR: new_pos = (int32_t)h->pos + (int32_t)pos;      break;
    case LV_FS_SEEK_END: new_pos = (int32_t)h->entry->size + (int32_t)pos; break;
    default: return LV_FS_RES_INV_PARAM;
    }

    if (new_pos < 0) new_pos = 0;
    if ((uint32_t)new_pos > h->entry->size) new_pos = (int32_t)h->entry->size;

    h->pos = (uint32_t)new_pos;
    return LV_FS_RES_OK;
}

/**
 * @brief  获取当前读写位置
 */
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    LV_UNUSED(drv);

    file_handle_t *h = (file_handle_t *)file_p;
    if (!h) return LV_FS_RES_INV_PARAM;

    *pos_p = h->pos;
    return LV_FS_RES_OK;
}

/**
 * @brief  关闭文件，释放句柄
 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);

    if (file_p) {
        lv_free(file_p);
    }
    return LV_FS_RES_OK;
}

/* ═══════════════════════════════════════════════════════════════
 * 初始化入口
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief  注册 W25Q64 LVGL 文件系统驱动
 * @note   驱动盘符固定为 'W'，路径示例 "W:/wallpaper.bin"
 */
void w25q64_fs_init(void)
{
    static bool registered = false;
    if (registered) return;
    registered = true;

    lv_fs_drv_t *fs_drv = lv_malloc(sizeof(lv_fs_drv_t));
    LV_ASSERT_MALLOC(fs_drv);
    if (!fs_drv) return;

    lv_fs_drv_init(fs_drv);
    fs_drv->letter    = 'W';
    fs_drv->cache_size = 0;
    fs_drv->open_cb   = fs_open;
    fs_drv->close_cb  = fs_close;
    fs_drv->read_cb   = fs_read;
    fs_drv->seek_cb   = fs_seek;
    fs_drv->tell_cb   = fs_tell;

    lv_fs_drv_register(fs_drv);
}
