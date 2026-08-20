#include "app_lcd.h"
#include "bsp_LCD_ILI9341.h"
#include "current.h"
#include "servo_calib.h"
#include "servo.h"
#include "machine.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "tim.h"
#include "lvgl.h"
#include "lv_demos.h"

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
static lv_obj_t *meter_cur;             /* 电流表盘 */
static lv_obj_t *lbl_meter_val;         /* 表盘中心数值 "0.0000 A" */
static lv_meter_indicator_t *meter_cur_needle;
static int32_t meter_needle_cur = 0;    /* 指针动画起始值 */
static lv_obj_t *lbl_state;
static lv_obj_t *lbl_state_dot;      /* 状态同色圆点 */
static lv_obj_t *lbl_ch;
static lv_obj_t *lbl_deg;
static lv_obj_t *lbl_up;
static lv_obj_t *lbl_lo;
static lv_obj_t *lbl_time;
static lv_obj_t *bar_calib;
static lv_obj_t *lbl_calib_ratio;   /* 显示 "3/17" 之类 */
static lv_obj_t *btn_machine;       /* 顶部机型切换按钮 */

static char g_buf[32];
static char meter_tick_label[8];   /* 表盘刻度标签临时缓冲区（安培一位小数） */

static lv_obj_t *ui_make_label(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y,
                               const lv_font_t *font, lv_color_t color)
{
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, txt);
  lv_obj_set_pos(lbl, x, y);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, color, 0);
  return lbl;
}  

/* 圆角白卡片（信息分组背景） */
static lv_obj_t *ui_make_card(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card, lv_color_white(), 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xD8D8D8), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(card, 6, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
  lv_obj_set_style_shadow_ofs_y(card, 2, 0);
  return card;
}

/* 状态文字/圆点配色：按扫频阶段区分 */
static lv_color_t state_color(CalibState_t s)
{
  switch (s)
  {
  case CALIB_IDLE:        return lv_palette_main(LV_PALETTE_GREEN);
  case CALIB_SEARCH_UP:   return lv_palette_main(LV_PALETTE_BLUE);
  case CALIB_SEARCH_DOWN: return lv_palette_main(LV_PALETTE_BLUE);
  case CALIB_TRANSITION:  return lv_palette_main(LV_PALETTE_ORANGE);
  case CALIB_NEXT_SERVO:  return lv_palette_main(LV_PALETTE_AMBER);
  case CALIB_DONE:        return lv_palette_main(LV_PALETTE_PURPLE);
  default:                return lv_palette_main(LV_PALETTE_GREY);
  }
}

/* 表盘刻度标签：内部 0~2000(mA) → 按安培一位小数显示（0.0/0.5/1.0/1.5/2.0） */
static void meter_scale_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_DRAW_PART_BEGIN) {
    lv_obj_draw_part_dsc_t *p = lv_event_get_param(e);
    /* 仅主刻度（带标签，text 非空）改写文本；次要刻度 text 为 NULL */
    if (p->part == LV_PART_TICKS && p->type == LV_METER_DRAW_PART_TICK && p->text != NULL) {
      sprintf(meter_tick_label, "%d.%d",
              (int)(p->value / 1000), (int)((p->value / 100) % 10));
      p->text = meter_tick_label;
    }
  }
}

/* 切换成功后立即刷新机型按钮文字 */
static void machine_label_refresh(void)
{
  if (btn_machine) {
    lv_obj_t *lbl = lv_obj_get_child(btn_machine, 0);
    lv_label_set_text(lbl, Machine_GetCurrent()->name);
  }
}

/* 机型选择 msgbox：LV_EVENT_VALUE_CHANGED —— 用户点中了某个机型
   事件从内部按钮矩阵冒泡到 msgbox，需用 current_target 取到 msgbox 本身 */
static void machine_msgbox_cb(lv_event_t *e)
{
  lv_obj_t *mbox = lv_event_get_current_target(e);
  uint16_t ab = lv_msgbox_get_active_btn(mbox);
  int r = Machine_Switch(ab);

  if (r == 0) {
    lv_msgbox_close(mbox);                 /* 切换成功：关菜单 */
    machine_label_refresh();               /* 按钮文字更新为新机型 */
  }
  else if (r == -1) {
    /* 标定进行中禁止切换 —— 本版本无 lv_msgbox_set_text，改 label */
    lv_label_set_text(lv_msgbox_get_text(mbox),
                      "Calibrating... stop first");
  }
  /* r==-2（越界）不应发生，忽略 */
}

/* 顶部机型按钮被点击：弹出机型选择菜单 */
static void machine_btn_click_cb(lv_event_t *e)
{
  (void)e;
  const uint8_t per_row = 2;   /* 每行机型数：小字号按钮放 2 个，留足文字宽度 */
  /* 容量 = 机型名 + 换行符 + 终止 NULL，上限按最坏情况留足 */
  static const char *btn_txts[MACHINE_PROFILE_COUNT + MACHINE_PROFILE_COUNT / 2 + 1];
  uint8_t i, n = 0, rows = 1;

  /* 从档案取机型名，每 per_row 个插入 "\n" 让按钮矩阵自动分行 */
  for (i = 0; i < MACHINE_PROFILE_COUNT; i++) {
    if (i > 0 && (i % per_row) == 0) {
      btn_txts[n++] = "\n";
      rows++;
    }
    btn_txts[n++] = machine_profiles[i].name;
  }
  btn_txts[n] = NULL;

  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), "Select Model",
                                    "Tap a machine to switch:", btn_txts, true);
  lv_obj_center(mbox);
  lv_obj_set_width(mbox, 230);   /* 屏幕 240px，收窄避免超出 */

  /* 机型多：用小字号 + 按钮矩阵按行数重算高度 */
  lv_obj_t *btns = lv_msgbox_get_btns(mbox);
  lv_obj_set_style_text_font(btns, &lv_font_montserrat_12, LV_PART_ITEMS);
  lv_obj_set_style_pad_row(btns, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_column(btns, 4, LV_PART_MAIN);
  lv_obj_set_width(btns, lv_pct(100));
  lv_coord_t btn_h = lv_font_get_line_height(&lv_font_montserrat_12) + 12;
  lv_obj_set_height(btns, rows * btn_h + (rows - 1) * 6);

  lv_obj_add_event_cb(mbox, machine_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_create(void)
{
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);   /* 浅色主题：浅灰底 */

  /* ---- 顶部机型条 ---- */
  lv_obj_t *bar_top = lv_obj_create(scr);
  lv_obj_set_size(bar_top, 240, 24);
  lv_obj_set_pos(bar_top, 0, 0);
  lv_obj_set_style_bg_color(bar_top, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(bar_top, 0, 0);
  lv_obj_set_style_border_width(bar_top, 0, 0);
  lv_obj_set_style_pad_all(bar_top, 0, 0);
  lv_obj_clear_flag(bar_top, LV_OBJ_FLAG_SCROLLABLE);

  ui_make_label(bar_top, "MODEL:", 10, 5, &lv_font_montserrat_12, lv_color_white());

  btn_machine = lv_btn_create(bar_top);
  lv_obj_set_size(btn_machine, 100, 20);
  lv_obj_set_pos(btn_machine, 62, 2);
  lv_obj_set_style_bg_color(btn_machine, lv_palette_darken(LV_PALETTE_BLUE, 2), 0);
  lv_obj_set_style_radius(btn_machine, 4, 0);
  lv_obj_set_style_pad_all(btn_machine, 0, 0);
  lv_obj_add_event_cb(btn_machine, machine_btn_click_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_machine = lv_label_create(btn_machine);
  lv_label_set_text(lbl_machine, Machine_GetCurrent()->name);
  lv_obj_center(lbl_machine);

  /* 电流表盘（lv_meter 指针式仪表，量程 0~2000mA） */
  meter_cur = lv_meter_create(scr);
  lv_obj_set_size(meter_cur, 132, 132);
  lv_obj_set_pos(meter_cur, 54, 28);
  lv_obj_add_flag(meter_cur, LV_OBJ_FLAG_OVERFLOW_VISIBLE);   /* 防刻度标签被裁剪 */
  lv_obj_set_style_pad_all(meter_cur, 5, LV_PART_MAIN);       /* 收窄内边距，表盘更大 */

  lv_meter_scale_t *scale = lv_meter_add_scale(meter_cur);
  lv_meter_set_scale_range(meter_cur, scale, 0, 2000, 270, 135);   /* 0~2000mA, 270° 弧 */
  lv_meter_set_scale_ticks(meter_cur, scale, 21, 2, 8, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meter_cur, scale, 5, 3, 12, lv_color_hex(0x333333), 8);
  /* 刻度值：黑字 12 号，浅色底上清晰 */
  lv_obj_set_style_text_color(meter_cur, lv_color_black(), LV_PART_TICKS);
  lv_obj_set_style_text_font(meter_cur, &lv_font_montserrat_12, LV_PART_TICKS);
  /* 刻度标签改为安培一位小数显示 */
  lv_obj_add_event_cb(meter_cur, meter_scale_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

  meter_cur_needle = lv_meter_add_needle_line(meter_cur, scale, 3,
                        lv_palette_main(LV_PALETTE_BLUE), -12);
  lv_meter_set_indicator_value(meter_cur, meter_cur_needle, 0);

  /* 分区着色：低电流绿(0~0.6A) / 中电流黄(0.6~1A) / 高电流红(1~2A) */
  lv_meter_indicator_t *zone;
  zone = lv_meter_add_scale_lines(meter_cur, scale, lv_palette_main(LV_PALETTE_GREEN),
                                  lv_palette_main(LV_PALETTE_GREEN), false, 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 0);
  lv_meter_set_indicator_end_value(meter_cur, zone, 600);
  zone = lv_meter_add_arc(meter_cur, scale, 3, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 0);
  lv_meter_set_indicator_end_value(meter_cur, zone, 600);

  zone = lv_meter_add_scale_lines(meter_cur, scale, lv_palette_darken(LV_PALETTE_YELLOW, 1),
                                  lv_palette_darken(LV_PALETTE_YELLOW, 1), false, 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 600);
  lv_meter_set_indicator_end_value(meter_cur, zone, 1000);
  zone = lv_meter_add_arc(meter_cur, scale, 3, lv_palette_darken(LV_PALETTE_YELLOW, 1), 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 600);
  lv_meter_set_indicator_end_value(meter_cur, zone, 1000);

  zone = lv_meter_add_scale_lines(meter_cur, scale, lv_palette_main(LV_PALETTE_RED),
                                  lv_palette_main(LV_PALETTE_RED), false, 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 1000);
  lv_meter_set_indicator_end_value(meter_cur, zone, 2000);
  zone = lv_meter_add_arc(meter_cur, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
  lv_meter_set_indicator_start_value(meter_cur, zone, 1000);
  lv_meter_set_indicator_end_value(meter_cur, zone, 2000);

  /* 指针轴心圆（LV_PART_INDICATOR），参照 lv_demo_widgets.c 的 needle pivot */
  lv_obj_set_style_size(meter_cur, 10, LV_PART_INDICATOR);
  lv_obj_set_style_radius(meter_cur, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(meter_cur, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(meter_cur, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);

  lbl_meter_val = lv_label_create(meter_cur);
  lv_label_set_text(lbl_meter_val, "0.00");
  lv_obj_align(lbl_meter_val, LV_ALIGN_CENTER, 0, 40);   /* 轴心下方，弧线空档处 */
  lv_obj_set_style_text_font(lbl_meter_val, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_meter_val, lv_palette_darken(LV_PALETTE_GREEN, 2), 0);

  /* ---- 卡1：状态 + 扫频进度 ---- */
  lv_obj_t *card1 = ui_make_card(10, 166, 220, 58);

  lbl_state_dot = lv_obj_create(card1);                      /* 状态同色圆点 */
  lv_obj_set_size(lbl_state_dot, 8, 8);
  lv_obj_set_pos(lbl_state_dot, 12, 12);
  lv_obj_set_style_radius(lbl_state_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(lbl_state_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(lbl_state_dot, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_set_style_border_width(lbl_state_dot, 0, 0);
  lv_obj_set_style_pad_all(lbl_state_dot, 0, 0);

  lbl_state = ui_make_label(card1, "IDLE", 26, 8, &lv_font_montserrat_14,
                            lv_palette_main(LV_PALETTE_GREEN));

  bar_calib = lv_bar_create(card1);                          /* 胶囊进度条 */
  lv_obj_set_size(bar_calib, 140, 16);
  lv_obj_set_pos(bar_calib, 12, 30);
  lv_bar_set_range(bar_calib, 0, 100);
  lv_bar_set_value(bar_calib, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(bar_calib, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_calib, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar_calib, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
  lv_obj_set_style_border_color(bar_calib, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
  lv_obj_set_style_border_width(bar_calib, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_calib, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
  lbl_calib_ratio = ui_make_label(card1, "0/17", 160, 32, &lv_font_montserrat_14,
                                  lv_palette_darken(LV_PALETTE_GREEN, 2));

  /* ---- 卡2：CH / Deg / Up / Lo / Time ---- */
  lv_obj_t *card2 = ui_make_card(10, 230, 220, 80);

  ui_make_label(card2, "CH:", 12, 8, &lv_font_montserrat_18, lv_color_black());
  lbl_ch = ui_make_label(card2, "0", 55, 8, &lv_font_montserrat_18,
                         lv_palette_darken(LV_PALETTE_GREEN, 2));
  ui_make_label(card2, "Deg:", 105, 8, &lv_font_montserrat_18, lv_color_black());
  lbl_deg = ui_make_label(card2, "0", 165, 8, &lv_font_montserrat_18,
                          lv_palette_darken(LV_PALETTE_GREEN, 2));

  ui_make_label(card2, "Up:", 12, 34, &lv_font_montserrat_14, lv_color_black());
  lbl_up = ui_make_label(card2, "0", 40, 34, &lv_font_montserrat_14,
                         lv_palette_darken(LV_PALETTE_GREEN, 2));
  ui_make_label(card2, "Lo:", 105, 34, &lv_font_montserrat_14, lv_color_black());
  lbl_lo = ui_make_label(card2, "0", 130, 34, &lv_font_montserrat_14,
                         lv_palette_darken(LV_PALETTE_GREEN, 2));

  ui_make_label(card2, "Time:", 12, 54, &lv_font_montserrat_14, lv_color_black());
  lbl_time = ui_make_label(card2, "0s", 70, 54, &lv_font_montserrat_14,
                           lv_palette_darken(LV_PALETTE_CYAN, 3));
}

static void ui_update_cb(lv_timer_t *timer)
{
  (void)timer;

  lv_color_t st_col = state_color(calib_state);
  lv_obj_set_style_bg_color(lbl_state_dot, st_col, 0);
  lv_obj_set_style_text_color(lbl_state, st_col, 0);
  lv_label_set_text(lbl_state, CalibState_ToStr(calib_state));

  /* 钳位：切机型瞬间 cur_idx 可能越界，读表前归零 */
  uint8_t n = Machine_GetServoNum();
  uint8_t idx = (cur_idx < n) ? cur_idx : 0;

  sprintf(g_buf, "%u", (unsigned)idx);
  lv_label_set_text(lbl_ch, g_buf);
  sprintf(g_buf, "%u", (unsigned)cur_angle);
  lv_label_set_text(lbl_deg, g_buf);

  sprintf(g_buf, "%u", (unsigned)servo_limits[idx].upper_limit_deg);
  lv_label_set_text(lbl_up, g_buf);
  sprintf(g_buf, "%u", (unsigned)servo_limits[idx].lower_limit_deg);
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

static void meter_needle_anim_cb(void *var, int32_t v)
{
    lv_meter_set_indicator_value(meter_cur, var, v);
}

static void meter_update_cb(lv_timer_t *timer)
{
    (void)timer;
    int32_t target = (int32_t)(Current_GetValue() * 1000.0f);
    lv_anim_del(meter_cur_needle, meter_needle_anim_cb);   /* 取消上一个动画 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, meter_cur_needle);
    lv_anim_set_exec_cb(&a, meter_needle_anim_cb);
    lv_anim_set_values(&a, meter_needle_cur, target);
    lv_anim_set_time(&a, 300);
    lv_anim_start(&a);
    meter_needle_cur = target;

    sprintf(g_buf, "%.2f", Current_GetValue());
    lv_label_set_text(lbl_meter_val, g_buf);
}

void LCD_TASK_ENTRY(void *argument)
{
  /* USER CODE BEGIN LCD_TASK_ENTRY */

  /* ---- LVGL 显示（当前生效） ---- */
  ui_create();
  lv_timer_create(ui_update_cb, 500, NULL);
  lv_timer_create(meter_update_cb, 200, NULL);

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
