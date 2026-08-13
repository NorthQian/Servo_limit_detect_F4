#include "app_lcd.h"
#include "app_relay.h"
#include "bsp_LCD_ILI9341.h"
#include "current.h"
#include "servo_calib.h"
#include "servo.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "tim.h"

extern uint8_t cur_idx;
extern uint16_t cur_angle;
extern CalibState_t calib_state;
extern ServoLimit_t servo_limits[SERVO_NUM];

static const char *CalibState_ToStr(CalibState_t s)
{
    switch (s) {
    case CALIB_IDLE:        return "IDLE";
    case CALIB_SEARCH_UP:   return "SEARCH_UP";
    case CALIB_TRANSITION:  return "TRANSITION";
    case CALIB_SEARCH_DOWN: return "SEARCH_DOWN";
    case CALIB_NEXT_SERVO:  return "NEXT_SERVO";
    case CALIB_DONE:        return "DONE";
    default:                return "UNKNOWN";
    }
}

void LCD_TASK_ENTRY(void *argument)
{
  /* USER CODE BEGIN LCD_TASK_ENTRY */

  char buf[32];
  float current;

  osDelay(500);

  /* 清屏 */
  LCD_Fill(0, 0, LCD_GetWidth() - 1, LCD_GetHeight() - 1, BLACK);

  /* 标题 */
  LCD_String(10, 10,  "Noetix Robotics", 16, YELLOW, BLACK);
  LCD_Line(10, 35, 230, 35, BLUE);

  /* 静态标签 */
  LCD_String(10, 140, "Relay:", 16, WHITE, BLACK);

  /* 状态标签 */
  LCD_String(10, 170, "State:", 16, WHITE, BLACK);

  /* 标定区域静态标签 */
  LCD_Line(10, 200, 230, 200, BLUE);
  LCD_String(10, 210, "CALIB", 16, YELLOW, BLACK);
  LCD_String(10, 235, "CH:",  24, WHITE, BLACK);
  LCD_String(100,235, "Deg:", 24, WHITE, BLACK);
  LCD_String(10,  265, "Up:", 16, WHITE, BLACK);
  LCD_String(100, 265, "Lo:", 16, WHITE, BLACK);

  /* 计时器静态标签 */
  LCD_String(10,  295, "Time:", 16, WHITE, BLACK);

  /* Infinite loop */
  for (;;)
  {
    /* 读取Current_Task已更新的电流值 */
    current = Current_GetValue();

    /* 清掉上次的值 */
    LCD_Fill(10, 50, 230, 120, BLACK);

    /* 显示电流值 */
    sprintf(buf, "I = %.4f A", current);
    LCD_String(10, 60, buf, 24, GREEN, BLACK);

    /* 继电器状态（只更新 ON/OFF） */
    LCD_Fill(90, 140, 230, 160, BLACK);
    LCD_String(90, 140, Relay_GetState() ? "ON " : "OFF", 16,
               Relay_GetState() ? GREEN : RED, BLACK);

    /* 标定状态刷新 */
    LCD_Fill(80, 170, 200, 190, BLACK);
    LCD_String(80, 170, (char *)CalibState_ToStr(calib_state), 16, GREEN, BLACK);

    /* 标定数据动态刷新 */
    LCD_Fill(45,  235, 90,  263, BLACK);
    LCD_Fill(150, 235, 195, 263, BLACK);
    sprintf(buf, "%d", cur_idx);
    LCD_String(45,  235, buf, 24, GREEN, BLACK);
    sprintf(buf, "%d", cur_angle);
    LCD_String(150, 235, buf, 24, GREEN, BLACK);

    /* 上下限度刷新 */
    LCD_Fill(40,  265, 90,  285, BLACK);
    LCD_Fill(130, 265, 180, 285, BLACK);
    sprintf(buf, "%u", servo_limits[cur_idx].upper_limit_deg);
    LCD_String(40,  265, buf, 16, GREEN, BLACK);
    sprintf(buf, "%u", servo_limits[cur_idx].lower_limit_deg);
    LCD_String(130, 265, buf, 16, GREEN, BLACK);

    /* 计时器值刷新（DONE 时不刷新，保持最终值） */
    if (calib_state != CALIB_IDLE && calib_state != CALIB_DONE) {
        LCD_Fill(70, 295, 130, 310, BLACK);
        sprintf(buf, "%lus", SweepTimer_GetElapsedSec());
        LCD_String(70, 295, buf, 16, CYAN, BLACK);
    }

    osDelay(500);
  }
  /* USER CODE END LCD_TASK_ENTRY */
}
