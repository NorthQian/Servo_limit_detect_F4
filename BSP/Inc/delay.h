/**
 ******************************************************************************
 * @file    delay.h
 * @brief   Microsecond and millisecond delay functions (HAL-compatible)
 * @note    Uses DWT cycle counter for us delay, HAL_Delay for ms delay
 ******************************************************************************
 */

#ifndef __DELAY_H
#define __DELAY_H

#include "bsp.h"


void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);

#endif /* __DELAY_H */
