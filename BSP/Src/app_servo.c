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

void App_Servo(void *argument)
{
  /* USER CODE BEGIN App_Servo */

  osSemaphoreAcquire(Key2SemaHandle, osWaitForever);

  /* Infinite loop */
  for (;;)
  {
    for (cur_idx = 0; cur_idx < CALIB_SERVO_COUNT; cur_idx++)
    {

      switch (calib_state)
      {
      case CALIB_IDLE:
        cur_angle = servo_limits[cur_idx].center_deg;
        break;
      case CALIB_SEARCH_UP:

        if (Ina229.Current < 0.6f)
        {

          cur_angle += CALIB_STEP_UP_DEG;


          if (cur_angle >= servo_limits[cur_idx].upper_limit_deg)
          {
            servo_limits[cur_idx].upper_limit_deg = cur_angle;
          }
        }else{

          calib_state = CALIB_TRANSITION;
        }
        break;
      case CALIB_TRANSITION:
        cur_angle = servo_limits[cur_idx].center_deg;
        calib_state = CALIB_SEARCH_DOWN;
        break;
      case CALIB_SEARCH_DOWN:
        if (Ina229.Current < 0.6f)
        {

          cur_angle -= CALIB_STEP_DOWN_DEG;

          if (cur_angle <= servo_limits[cur_idx].lower_limit_deg)
          {
            servo_limits[cur_idx].lower_limit_deg = cur_angle;
          }
        }else{

          calib_state = CALIB_NEXT_SERVO;
        }
        break;
      default:
        break;
      }

      osDelay(50);
    }
  }
  /* USER CODE END App_Servo */
}
