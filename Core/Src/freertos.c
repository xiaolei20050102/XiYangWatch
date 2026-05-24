/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "app.h"
#include "Framework/gesture.h"

/* 前向声明：避开 touch_cst816s.h 与 gesture.h 的枚举名冲突 */
extern uint8_t CST816_GetGesture(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for LvglTask */
osThreadId_t lvglTaskHandle;
const osThreadAttr_t lvglTask_attributes = {
  .name = "lvglTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartLvglTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  lvglTaskHandle = osThreadNew(StartLvglTask, NULL, &lvglTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

static void btn_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED) {
    bool pressed = (code == LV_EVENT_PRESSED);
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_set_style_bg_color(btn, pressed ? lv_color_white() : lv_color_hex(0x333333), 0);
  }
}

void StartLvglTask(void *argument)
{
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();

  extern const lv_font_t cascadia_mono_16_1bpp;

  lv_theme_t * th = lv_theme_mono_init(lv_display_get_default(), true, &cascadia_mono_16_1bpp);
  lv_display_set_theme(lv_display_get_default(), th);

  app_init();

  for(;;)
  {
    lv_timer_handler();
    app_loop();

    /* 触摸手势 → 页面导航 */
    uint8_t raw = CST816_GetGesture();
    gesture_t g = GESTURE_NONE;
    switch (raw) {
        case 0x01: g = GESTURE_DOWN;  break;
        case 0x02: g = GESTURE_UP;    break;
        case 0x03: g = GESTURE_LEFT;  break;
        case 0x04: g = GESTURE_RIGHT; break;
        case 0x05: g = GESTURE_CLICK; break;
        case 0x0C: g = GESTURE_LONGPRESS; break;
    }
    gesture_feed(g);

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

