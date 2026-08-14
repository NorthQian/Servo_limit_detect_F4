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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
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
/* Definitions for LCD_TASK */
osThreadId_t LCD_TASKHandle;
const osThreadAttr_t LCD_TASK_attributes = {
  .name = "LCD_TASK",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Current_Task */
osThreadId_t Current_TaskHandle;
uint32_t Current_TaskBuffer[ 128 ];
osStaticThreadDef_t Current_TaskControlBlock;
const osThreadAttr_t Current_Task_attributes = {
  .name = "Current_Task",
  .cb_mem = &Current_TaskControlBlock,
  .cb_size = sizeof(Current_TaskControlBlock),
  .stack_mem = &Current_TaskBuffer[0],
  .stack_size = sizeof(Current_TaskBuffer),
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for RELAY_TASK */
osThreadId_t RELAY_TASKHandle;
const osThreadAttr_t RELAY_TASK_attributes = {
  .name = "RELAY_TASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for KEY_SCAN */
osThreadId_t KEY_SCANHandle;
const osThreadAttr_t KEY_SCAN_attributes = {
  .name = "KEY_SCAN",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for SWEEP */
osThreadId_t SWEEPHandle;
uint32_t SWEEPBuffer[ 256 ];
osStaticThreadDef_t SWEEPControlBlock;
const osThreadAttr_t SWEEP_attributes = {
  .name = "SWEEP",
  .cb_mem = &SWEEPControlBlock,
  .cb_size = sizeof(SWEEPControlBlock),
  .stack_mem = &SWEEPBuffer[0],
  .stack_size = sizeof(SWEEPBuffer),
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for Key1Sema */
osSemaphoreId_t Key1SemaHandle;
const osSemaphoreAttr_t Key1Sema_attributes = {
  .name = "Key1Sema"
};
/* Definitions for Key2Sema */
osSemaphoreId_t Key2SemaHandle;
const osSemaphoreAttr_t Key2Sema_attributes = {
  .name = "Key2Sema"
};
/* Definitions for Key3Sema */
osSemaphoreId_t Key3SemaHandle;
const osSemaphoreAttr_t Key3Sema_attributes = {
  .name = "Key3Sema"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void LCD_TASK_ENTRY(void *argument);
void Get_Current(void *argument);
void App_relay(void *argument);
void App_Key_Scan(void *argument);
void App_sweep_task(void *argument);

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

  /* Create the semaphores(s) */
  /* creation of Key1Sema */
  Key1SemaHandle = osSemaphoreNew(1, 0, &Key1Sema_attributes);

  /* creation of Key2Sema */
  Key2SemaHandle = osSemaphoreNew(1, 0, &Key2Sema_attributes);

  /* creation of Key3Sema */
  Key3SemaHandle = osSemaphoreNew(1, 0, &Key3Sema_attributes);

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
  /* creation of LCD_TASK */
  LCD_TASKHandle = osThreadNew(LCD_TASK_ENTRY, NULL, &LCD_TASK_attributes);

  /* creation of Current_Task */
  Current_TaskHandle = osThreadNew(Get_Current, NULL, &Current_Task_attributes);

  /* creation of RELAY_TASK */
  RELAY_TASKHandle = osThreadNew(App_relay, NULL, &RELAY_TASK_attributes);

  /* creation of KEY_SCAN */
  KEY_SCANHandle = osThreadNew(App_Key_Scan, NULL, &KEY_SCAN_attributes);

  /* creation of SWEEP */
  SWEEPHandle = osThreadNew(App_sweep_task, NULL, &SWEEP_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_LCD_TASK_ENTRY */
/**
  * @brief  Function implementing the LCD_TASK thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_LCD_TASK_ENTRY */
__weak void LCD_TASK_ENTRY(void *argument)
{
  /* USER CODE BEGIN LCD_TASK_ENTRY */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LCD_TASK_ENTRY */
}

/* USER CODE BEGIN Header_Get_Current */
/**
* @brief Function implementing the Current_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Get_Current */
__weak void Get_Current(void *argument)
{
  /* USER CODE BEGIN Get_Current */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Get_Current */
}

/* USER CODE BEGIN Header_App_relay */
/**
* @brief Function implementing the RELAY_TASK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_relay */
__weak void App_relay(void *argument)
{
  /* USER CODE BEGIN App_relay */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END App_relay */
}

/* USER CODE BEGIN Header_App_Key_Scan */
/**
* @brief Function implementing the KEY_SCAN thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_Key_Scan */
__weak void App_Key_Scan(void *argument)
{
  /* USER CODE BEGIN App_Key_Scan */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END App_Key_Scan */
}

/* USER CODE BEGIN Header_App_sweep_task */
/**
* @brief Function implementing the SWEEP thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_sweep_task */
__weak void App_sweep_task(void *argument)
{
  /* USER CODE BEGIN App_sweep_task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END App_sweep_task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

