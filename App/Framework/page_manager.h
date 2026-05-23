#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "../Pages/pages_config.h"

typedef enum {
    STATE_AT_HUB,
    STATE_AT_SPOKE,
    STATE_AT_OVERLAY,
} page_state_t;

typedef enum {
    SPOKE_LEFT  = 0,
    SPOKE_RIGHT,
    SPOKE_UP,
    SPOKE_DOWN,
} spoke_dir_t;

void page_manager_init(void);
void page_manager_go_home(void);
void page_manager_go_spoke(spoke_dir_t dir);
void page_manager_push(page_id_t id);
void page_manager_pop(void);
void page_manager_update(void);
page_state_t page_manager_get_state(void);

#endif
