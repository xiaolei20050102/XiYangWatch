#ifndef GESTURE_H
#define GESTURE_H

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_LEFT,
    GESTURE_RIGHT,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_CLICK,
    GESTURE_LONGPRESS,
} gesture_t;

void gesture_init(void);
void gesture_feed(gesture_t g);

#endif
