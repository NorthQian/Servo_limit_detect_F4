#include "app_servo.h"
#include "servo_calib.h"
#include "cmsis_os2.h"
#include "servo.h"
#include "servo_actuator.h"
#include "ina229.h"
#include "tim.h"
#include "bsp_log.h"
#include "machine.h"

extern osSemaphoreId_t Key1SemaHandle;
extern osSemaphoreId_t Key2SemaHandle;
extern osSemaphoreId_t Key3SemaHandle;

extern INA229 Ina229;

void App_sweep_task(void *argument)
{
  /* USER CODE BEGIN App_sweep_task */
  osSemaphoreAcquire(Key2SemaHandle, osWaitForever);
  osSemaphoreAcquire(Key3SemaHandle, 0); /* 清除检测前误触发的 KEY3 */
  SweepTimer_Start();
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); /* 扫频中：蓝灯亮 */
  /* Infinite loop */
  for (;;)
  {
    for (cur_idx = 0; cur_idx < Machine_GetServoNum(); /*cur_idx++*/)
    {

      switch (calib_state)
      {
      case CALIB_IDLE:
        if (Machine_IsServoSkipped(cur_idx)) {
            servo_limits[cur_idx].upper_limit_deg = CALIB_HARD_MAX;
            servo_limits[cur_idx].lower_limit_deg = CALIB_HARD_MIN;
            calib_state = CALIB_NEXT_SERVO;
            break;
        }
        cur_angle = servo_limits[cur_idx].center_deg;
        Servo_SendAngles((ServoChannel_t[]){{.channel = servo_limits[cur_idx].channel, .angle = cur_angle}}, 1, 500);
        osDelay(500);
        calib_state = CALIB_SEARCH_UP;
        break;
      case CALIB_SEARCH_UP:

        if (Ina229.Current < 0.6f && cur_angle < CALIB_HARD_MAX)
        {
          if (cur_angle + CALIB_STEP_UP_DEG <= CALIB_HARD_MAX) {
              cur_angle += CALIB_STEP_UP_DEG;
          } else {
              cur_angle = CALIB_HARD_MAX;
          }
          Servo_SendAngles((ServoChannel_t[]){{.channel = servo_limits[cur_idx].channel, .angle = cur_angle}}, 1, 50);
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
        Servo_SendAngles((ServoChannel_t[]){{.channel = servo_limits[cur_idx].channel, .angle = cur_angle}}, 1, 500);
        osDelay(500);
        calib_state = CALIB_SEARCH_DOWN;
        break;
      case CALIB_SEARCH_DOWN:
        if (Ina229.Current < 0.6f && cur_angle > CALIB_HARD_MIN)
        {
          if (cur_angle >= CALIB_HARD_MIN + CALIB_STEP_UP_DEG) {
              cur_angle -= CALIB_STEP_UP_DEG;
          } else {
              cur_angle = CALIB_HARD_MIN;
          }
          Servo_SendAngles((ServoChannel_t[]){{.channel = servo_limits[cur_idx].channel, .angle = cur_angle}}, 1, 50);
          osDelay(50);
        }
        else
        {
          servo_limits[cur_idx].lower_limit_deg = cur_angle;

          calib_state = CALIB_NEXT_SERVO;
        }
        break;
      case CALIB_NEXT_SERVO:
        if (!Machine_IsServoSkipped(cur_idx))
        {
            cur_angle = servo_limits[cur_idx].center_deg;
            Servo_SendAngles((ServoChannel_t[]){{.channel = servo_limits[cur_idx].channel, .angle = cur_angle}}, 1, 500);
            osDelay(500);
        }
        cur_idx++;
        if (cur_idx < Machine_GetServoNum()) {
            calib_state = CALIB_IDLE;
        } else {
            calib_state = CALIB_DONE;
        }
        break;
      case CALIB_DONE:
      {
          static uint32_t total_time_s = 0;
          if (!calib_report_pending) {
              total_time_s = SweepTimer_GetElapsedSec();
              Calib_ExportServoLimits();
              PrintLog("Sweep total time: %lus\r\n", total_time_s);
              HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET); /* 扫频完成：蓝灯灭 */
              calib_report_pending = 1;
          }
          else if (osSemaphoreAcquire(Key3SemaHandle, 0) == osOK) {
              /* 检测完成后按 KEY3：再次上传标定结果 */
              Calib_ExportServoLimits();
              PrintLog("Sweep total time: %lus\r\n", total_time_s);
          }
          osDelay(100);
          break;
      }
      default:

        break;
      }

      osDelay(50);
    }
    osDelay(1);
  }
  /* USER CODE END App_sweep_task */
}