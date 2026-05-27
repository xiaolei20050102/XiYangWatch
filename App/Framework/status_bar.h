#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include "lvgl.h"

void status_bar_create(lv_obj_t *parent);
void status_bar_bring_to_front(void);
void status_bar_set_visible(bool visible);
void status_bar_refresh_time(void);
void status_bar_refresh_battery(void);

#endif
