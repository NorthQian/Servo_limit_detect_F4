#include "app_servo.h"
#include "servo_calib.h"
#include "cmsis_os2.h"
#include "servo.h"
#include "servo_actuator.h"
#include "ina229.h"

extern osSemaphoreId_t Key1SemaHandle;
extern osSemaphoreId_t Key2SemaHandle;
extern osSemaphoreId_t Key3SemaHandle;

extern uint8_t cur_idx;
extern uint16_t cur_angle;
extern CalibState_t calib_state;
extern ServoLimit_t servo_limits[SERVO_NUM];
extern INA229 Ina229;

void App_sweep_task(void *argument)
{
  /* USER CODE BEGIN App_sweep_task */
  osSemaphoreAcquire(Key2SemaHandle, osWaitForever);
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
  /* Infinite loop */
  for (;;)
  {
    for (cur_idx = 0; cur_idx < CALIB_SERVO_COUNT; /*cur_idx++*/)
    {

      switch (calib_state)
      {
      case CALIB_IDLE:
        cur_angle = servo_limits[cur_idx].center_deg;
        Servo_SendAngles((ServoChannel_t[]){{.channel = cur_idx, .angle = cur_angle}}, 1, 500);
        osDelay(500);
        calib_state = CALIB_SEARCH_UP;
        break;
      case CALIB_SEARCH_UP:

        if (Ina229.Current < 0.6f)
        {

          cur_angle += CALIB_STEP_UP_DEG;
          Servo_SendAngles((ServoChannel_t[]){{.channel = cur_idx, .angle = cur_angle}}, 1, 50);
          osDelay(50);
        }
        else
        {
          servo_limits[cur_idx].upper_limit_deg = cur_angle;

          calib_state = CALIB_TRANSITION;
        }
        break;
      case CALIB_TRANSITION:
        cur_angle = servo_limits[cur_idx].center_deg;
        Servo_SendAngles((ServoChannel_t[]){{.channel = cur_idx, .angle = cur_angle}}, 1, 500);
        osDelay(500);
        calib_state = CALIB_SEARCH_DOWN;
        break;
      case CALIB_SEARCH_DOWN:
        if (Ina229.Current < 0.6f)
        {

          cur_angle -= CALIB_STEP_DOWN_DEG;
          Servo_SendAngles((ServoChannel_t[]){{.channel = cur_idx, .angle = cur_angle}}, 1, 50);
          osDelay(50);
        }
        else
        {
          servo_limits[cur_idx].lower_limit_deg = cur_angle;

          calib_state = CALIB_NEXT_SERVO;
        }
        break;
      case CALIB_NEXT_SERVO:
        osDelay(1);
        break;
      default:

        break;
      }

      osDelay(50);
    }
    osDelay(1);
  }
  /* USER CODE END App_sweep_task */
}