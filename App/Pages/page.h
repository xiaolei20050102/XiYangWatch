#ifndef PAGE_H
#define PAGE_H

#include "lvgl.h"

typedef enum {
    PAGE_TYPE_HUB,
    PAGE_TYPE_SPOKE,
    PAGE_TYPE_OVERLAY
} page_type_t;

typedef struct {
    const char  *name;
    page_type_t  type;
    lv_obj_t   *(*create)(lv_obj_t *parent);
    void        (*destroy)(void);
    void        (*update)(void);
} page_t;

#endif
