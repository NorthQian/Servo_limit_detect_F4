#include "app_lcd.h"
#include "app_relay.h"
#include "bsp_LCD_ILI9341.h"
#include "current.h"
#include "servo_calib.h"
#include "stdio.h"
#include "cmsis_os2.h"

extern uint8_t cur_idx;
extern uint16_t cur_angle;

void LCD_TASK_ENTRY(void *argument)
{
  /* USER CODE BEGIN LCD_TASK_ENTRY */

  char buf[32];
  float current;

  osDelay(500);

  /* 清屏 */
  LCD_Fill(0, 0, LCD_GetWidth() - 1, LCD_GetHeight() - 1, BLACK);

  /* 标题 */
  LCD_String(10, 10,  "INA229 Current", 16, YELLOW, BLACK);
  LCD_Line(10, 35, 230, 35, BLUE);

  /* 静态标签 */
  LCD_String(10, 140, "Relay:", 24, WHITE, BLACK);

  /* 标定区域静态标签 */
  LCD_Line(10, 185, 230, 185, BLUE);
  LCD_String(10, 195, "CALIB", 16, YELLOW, BLACK);
  LCD_String(10, 220, "CH:",  24, WHITE, BLACK);
  LCD_String(100,220, "Deg:", 24, WHITE, BLACK);

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
    LCD_Fill(120, 140, 230, 170, BLACK);
    LCD_String(120, 140, Relay_GetState() ? "ON " : "OFF", 24,
               Relay_GetState() ? GREEN : RED, BLACK);

    /* 标定数据动态刷新 */
    LCD_Fill(45,  220, 90,  248, BLACK);
    LCD_Fill(150, 220, 195, 248, BLACK);
    sprintf(buf, "%d", cur_idx);
    LCD_String(45,  220, buf, 24, GREEN, BLACK);
    sprintf(buf, "%d", cur_angle);
    LCD_String(150, 220, buf, 24, GREEN, BLACK);

    osDelay(500);
  }
  /* USER CODE END LCD_TASK_ENTRY */
}
