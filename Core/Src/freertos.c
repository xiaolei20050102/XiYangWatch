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

  lv_obj_t * scr = lv_screen_active();
  lv_obj_set_style_pad_all(scr, 4, 0);

  lv_obj_t * lb;
  lv_obj_t * obj;

  /* ── 1. 文字 ── */
  lb = lv_label_create(scr);
  lv_label_set_text(lb, "1.Text: ABCabc 12345 @XiYang");
  lv_obj_set_pos(lb, 4, 4);

  /* ── 2. 矩形 + 圆角矩形（用 btn widget，自带可见样式）── */
  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 80, 30);
  lv_obj_set_pos(obj, 4, 28);
  lv_obj_set_style_radius(obj, 0, 0);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 80, 30);
  lv_obj_set_pos(obj, 92, 28);
  lv_obj_set_style_radius(obj, 8, 0);

  lb = lv_label_create(scr);
  lv_label_set_text(lb, "2.Rects: sharp / rounded");
  lv_obj_set_pos(lb, 4, 62);

  /* ── 3. 边框矩形（默认底 + 粗白边框）── */
  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 80, 30);
  lv_obj_set_pos(obj, 4, 82);
  lv_obj_set_style_border_width(obj, 3, 0);
  lv_obj_set_style_radius(obj, 4, 0);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 100, 30);
  lv_obj_set_pos(obj, 92, 82);
  lv_obj_set_style_border_width(obj, 3, 0);
  lv_obj_set_style_radius(obj, 15, 0);

  lb = lv_label_create(scr);
  lv_label_set_text(lb, "3.Borders: 3px / capsule");
  lv_obj_set_pos(lb, 4, 116);

  /* ── 4. 圆 + 点 ── */
  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 30, 30);
  lv_obj_set_pos(obj, 4, 140);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 8, 8);
  lv_obj_set_pos(obj, 42, 151);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);

  lb = lv_label_create(scr);
  lv_label_set_text(lb, "4.Circle & dot");
  lv_obj_set_pos(lb, 56, 145);

  /* ── 5. 触摸按钮 ── */
  obj = lv_button_create(scr);
  lv_obj_set_size(obj, 200, 40);
  lv_obj_set_pos(obj, 4, 180);
  lv_obj_set_style_radius(obj, 6, 0);

  lb = lv_label_create(obj);
  lv_label_set_text(lb, "5.Touch me!");
  lv_obj_center(lb);

  lb = lv_label_create(scr);
  lv_label_set_text(lb, "All OK? -> RTC + Watchface");
  lv_obj_set_pos(lb, 4, 228);

  for(;;)
  {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

