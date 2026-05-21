/**
 * @file lv_port_indev.c
 * @brief  LVGL 9.x 输入接口 —— CST816S 触摸
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "touch_cst816s.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(int32_t * x, int32_t * y);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t * indev_touchpad;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    touchpad_init();

    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void touchpad_init(void)
{
    CST816_Init();
}

static void touchpad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

    if (touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

static bool touchpad_is_pressed(void)
{
    uint8_t finger = CST816_GetFingerNum();
    return (finger != 0x00 && finger != 0xFF);
}

static void touchpad_get_xy(int32_t * x, int32_t * y)
{
    Touch_Info_t info;
    CST816_GetTouch(&info);
    *x = (int32_t)info.X_Pos;
    *y = (int32_t)info.Y_Pos;
}

#else

typedef int keep_pedantic_happy;
#endif
