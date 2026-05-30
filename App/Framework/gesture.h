#ifndef GESTURE_H
#define GESTURE_H

#include <stdbool.h>

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_LEFT,
    GESTURE_RIGHT,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_CLICK,
    GESTURE_LONGPRESS,
} gesture_t;

typedef bool (*gesture_intercept_cb_t)(gesture_t g);

void gesture_init(void);
void gesture_feed(gesture_t g);
void gesture_set_intercept(gesture_intercept_cb_t cb);
gesture_intercept_cb_t gesture_get_intercept(void);
bool gesture_was_recent(void);  /* true if a directional gesture fired within ~300ms */

#endif
