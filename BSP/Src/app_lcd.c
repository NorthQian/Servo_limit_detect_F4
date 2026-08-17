#include "app_lcd.h"
#include "app_relay.h"
#include "bsp_LCD_ILI9341.h"
#include "current.h"
#include "servo_calib.h"
#include "servo.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "tim.h"
#include "lvgl.h"
#include "lv_demos.h"

extern uint8_t cur_idx;
extern uint16_t cur_angle;
extern CalibState_t calib_state;
extern ServoLimit_t servo_limits[SERVO_NUM];

static const char *CalibState_ToStr(CalibState_t s)
{
  switch (s)
  {
  case CALIB_IDLE:
    return "IDLE";
  case CALIB_SEARCH_UP:
    return "SEARCH_UP";
  case CALIB_TRANSITION:
    return "TRANSITION";
  case CALIB_SEARCH_DOWN:
    return "SEARCH_DOWN";
  case CALIB_NEXT_SERVO:
    return "NEXT_SERVO";
  case CALIB_DONE:
    return "DONE";
  default:
    return "UNKNOWN";
  }
}

/* ---- UI 控件句柄 ---- */
static lv_obj_t *lbl_current;
static lv_obj_t *lbl_relay;
static lv_obj_t *lbl_state;
static lv_obj_t *lbl_ch;
static lv_obj_t *lbl_deg;
static lv_obj_t *lbl_up;
static lv_obj_t *lbl_lo;
static lv_obj_t *lbl_time;
static lv_obj_t *bar_calib;
static lv_obj_t *lbl_calib_ratio;   /* 显示 "3/17" 之类 */

static char g_buf[32];

static lv_obj_t *ui_make_label(const char *txt, lv_coord_t x, lv_coord_t y,
                               const lv_font_t *font, lv_color_t color)
{
  lv_obj_t *lbl = lv_label_create(lv_scr_act());
  lv_label_set_text(lbl, txt);
  lv_obj_set_pos(lbl, x, y);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, color, 0);
  return lbl;
}

static void ui_create(void)
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  ui_make_label("Noetix Robotics", 10, 10, &lv_font_montserrat_18,
                lv_palette_main(LV_PALETTE_YELLOW));

  lbl_current = ui_make_label("I = 0.0000 A", 10, 55, &lv_font_montserrat_18,
                              lv_palette_main(LV_PALETTE_GREEN));

  ui_make_label("Relay:", 10, 110, &lv_font_montserrat_14, lv_color_white());
  lbl_relay = ui_make_label("OFF", 90, 110, &lv_font_montserrat_14,
                            lv_palette_main(LV_PALETTE_RED));

  ui_make_label("State:", 10, 140, &lv_font_montserrat_14, lv_color_white());
  lbl_state = ui_make_label("IDLE", 90, 140, &lv_font_montserrat_14,
                            lv_palette_main(LV_PALETTE_GREEN));

  /* 校准进度条（替换原 "CALIB" 字样） */
  bar_calib = lv_bar_create(scr);
  lv_obj_set_size(bar_calib, 175, 14);
  lv_obj_set_pos(bar_calib, 10, 180);
  lv_bar_set_range(bar_calib, 0, 100);
  lv_bar_set_value(bar_calib, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_calib, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_color(bar_calib, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
  lv_obj_set_style_border_width(bar_calib, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_calib, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
  lbl_calib_ratio = ui_make_label("0/17", 190, 180, &lv_font_montserrat_14,
                                  lv_palette_main(LV_PALETTE_YELLOW));

  ui_make_label("CH:", 10, 210, &lv_font_montserrat_18, lv_color_white());
  lbl_ch = ui_make_label("0", 55, 210, &lv_font_montserrat_18,
                         lv_palette_main(LV_PALETTE_GREEN));
  ui_make_label("Deg:", 100, 210, &lv_font_montserrat_18, lv_color_white());
  lbl_deg = ui_make_label("0", 165, 210, &lv_font_montserrat_18,
                          lv_palette_main(LV_PALETTE_GREEN));

  ui_make_label("Up:", 10, 245, &lv_font_montserrat_14, lv_color_white());
  lbl_up = ui_make_label("0", 40, 245, &lv_font_montserrat_14,
                         lv_palette_main(LV_PALETTE_GREEN));
  ui_make_label("Lo:", 100, 245, &lv_font_montserrat_14, lv_color_white());
  lbl_lo = ui_make_label("0", 130, 245, &lv_font_montserrat_14,
                         lv_palette_main(LV_PALETTE_GREEN));

  ui_make_label("Time:", 10, 285, &lv_font_montserrat_14, lv_color_white());
  lbl_time = ui_make_label("0s", 70, 285, &lv_font_montserrat_14,
                           lv_palette_main(LV_PALETTE_CYAN));
}

static void ui_update_cb(lv_timer_t *timer)
{
  (void)timer;

  sprintf(g_buf, "I = %.4f A", Current_GetValue());
  lv_label_set_text(lbl_current, g_buf);

  lv_label_set_text(lbl_relay, Relay_GetState() ? "ON" : "OFF");
  lv_obj_set_style_text_color(lbl_relay,
      Relay_GetState() ? lv_palette_main(LV_PALETTE_GREEN)
                       : lv_palette_main(LV_PALETTE_RED), 0);

  lv_label_set_text(lbl_state, CalibState_ToStr(calib_state));

  sprintf(g_buf, "%u", (unsigned)cur_idx);
  lv_label_set_text(lbl_ch, g_buf);
  sprintf(g_buf, "%u", (unsigned)cur_angle);
  lv_label_set_text(lbl_deg, g_buf);

  sprintf(g_buf, "%u", (unsigned)servo_limits[cur_idx].upper_limit_deg);
  lv_label_set_text(lbl_up, g_buf);
  sprintf(g_buf, "%u", (unsigned)servo_limits[cur_idx].lower_limit_deg);
  lv_label_set_text(lbl_lo, g_buf);

  if (calib_state != CALIB_IDLE && calib_state != CALIB_DONE) {
    sprintf(g_buf, "%lus", (unsigned long)SweepTimer_GetElapsedSec());
    lv_label_set_text(lbl_time, g_buf);
  }

  /* 扫频进度：已扫频舵机数 / 总扫频舵机数（扣除跳过的舵机） */
  {
    uint8_t swept = Calib_GetSweptCount();
    uint8_t total = Calib_GetTotalSweptCount();
    lv_bar_set_value(bar_calib,
                     total ? (uint8_t)((uint32_t)swept * 100 / total) : 0,
                     LV_ANIM_ON);
    sprintf(g_buf, "%u/%u", (unsigned)swept, (unsigned)total);
    lv_label_set_text(lbl_calib_ratio, g_buf);
  }
}

void LCD_TASK_ENTRY(void *argument)
{
  /* USER CODE BEGIN LCD_TASK_ENTRY */

  /* ---- LVGL 显示（当前生效） ---- */
  ui_create();
  lv_timer_create(ui_update_cb, 500, NULL);

  /* ---- LVGL benchmark 演示 ---- */
#if 0
  /* 原简单 label 示例（保留备查） */
  lv_obj_t *label_title = lv_label_create(lv_scr_act());
  lv_label_set_text(label_title, "Noetix Robotics");
  lv_obj_align(label_title, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t *label_status = lv_label_create(lv_scr_act());
  lv_label_set_text(label_status, "LVGL OK");
  lv_obj_align(label_status, LV_ALIGN_CENTER, 0, 20);
#endif
#if 0
  /* 启动 widgets 演示 */
  lv_demo_widgets();
#endif
  /* LVGL 渲染循环（约 5ms 一次） */
  for (;;)
  {
    lv_timer_handler();
    osDelay(5);
  }

#if 0
  /* ---- 原直接绘制代码（保留备查） ---- */
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
#endif

  /* USER CODE END LCD_TASK_ENTRY */
}
