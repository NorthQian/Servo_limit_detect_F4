#ifndef  __BSP_XPT2046_H
#define  __BSP_XPT2046_H

/**
 * @file    bsp_XPT2046.h
 * @brief   XPT2046 触摸控制器驱动头文件（软件 SPI / 位翻转 GPIO）
 *
 * 移植自标准例程《29-1 显示屏_2.8寸_触摸测试_XPT2046》，去掉 W25Q128 校准数据
 * 存储依赖，校准参数改为硬编码默认值。仅支持 HAL 库。
 *
 * 引脚：CS=PD13, CLK=PE0, MOSI=PE2, MISO=PE3, IRQ(PEN)=PE4
 */

#include "stm32f4xx_hal.h"
#include "bsp_LCD_ILI9341.h"

/* 触摸引脚定义 */
#define XPT2046_IRQ_GPIO    GPIOE               /* 触摸信号指示引脚（不使用中断，轮询） */
#define XPT2046_IRQ_PIN     GPIO_PIN_4

#define XPT2046_CS_GPIO     GPIOD               /* 软件 SPI CS */
#define XPT2046_CS_PIN      GPIO_PIN_13

#define XPT2046_CLK_GPIO    GPIOE               /* 软件 SPI CLK */
#define XPT2046_CLK_PIN     GPIO_PIN_0

#define XPT2046_MOSI_GPIO   GPIOE               /* 软件 SPI MOSI */
#define XPT2046_MOSI_PIN    GPIO_PIN_2

#define XPT2046_MISO_GPIO   GPIOE               /* 软件 SPI MISO */
#define XPT2046_MISO_PIN    GPIO_PIN_3

/* 初始化 */
void      XPT2046_Init(uint16_t lcdWidth, uint16_t lcdHeight, uint8_t dir);

/* 触摸检测、获取坐标（接入 LVGL 时在 lv_port_indev.c 里调用） */
uint8_t   XPT2046_IsPressed(void);             /* 返回：0-未按下、1-按下中 */
uint16_t  XPT2046_GetX(void);                  /* 获取按下的 X 坐标 */
uint16_t  XPT2046_GetY(void);                  /* 获取按下的 Y 坐标 */

/* 触摸压力检测（本板 PENIRQ 未接出，改用 Z 压力判按下） */
uint16_t  XPT2046_GetPressure(void);                 /* 读触摸压力 4095-(Z2-Z1)，按压越大值越大 */

/* 触摸检测开关（非必需；初始化后默认开启） */
void      XPT2046_Cmd(uint8_t status);

/* 空钩子函数，供用户扩展 */
void      XPT2046_TouchDown(void);
void      XPT2046_TouchUp(void);

#endif /* __BSP_XPT2046_H */
