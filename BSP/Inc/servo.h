/**
 ******************************************************************************
 * @file    servo.h
 * @brief   Servo limit detection module — normalized servo instance with limits
 * @note    Upper/lower limits are derived from servo_config.h, normalizing
 *          start_deg/end_deg so upper_limit >= lower_limit always.
 ******************************************************************************
 */

#ifndef __SERVO_H
#define __SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Number of servos -----------------------------------------------------------*/
#define SERVO_NUM   27

/* Servo limit instance -------------------------------------------------------*/
typedef struct {
    uint8_t  channel;          /* Hardware channel index (0 ~ 35)            */
    uint16_t center_deg;       /* Center / default angle: temp_deg           */
    uint16_t start_deg;        /* Start limit angle (raw, from YAML)         */
    uint16_t end_deg;          /* End limit angle (raw, from YAML)           */
    uint16_t upper_limit_deg;  /* Upper limit: max(start_deg, end_deg)       */
    uint16_t lower_limit_deg;  /* Lower limit: min(start_deg, end_deg)       */
} ServoLimit_t;

/* Servo limit table (defined in servo.c) ------------------------------------*/
extern  ServoLimit_t servo_limits[SERVO_NUM];

/* API Functions --------------------------------------------------------------*/

/**
 * @brief  Get servo limit instance by servo ID
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1, corresponding to A0 ~ A26)
 * @return pointer to ServoLimit_t, or NULL if servo_id is out of range
 */
const ServoLimit_t *Servo_GetLimit(uint8_t servo_id);

/**
 * @brief  Check whether current angle has reached the upper limit
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg >= upper_limit_deg, 0 otherwise (or if invalid id)
 */
int Servo_IsAtUpperLimit(uint8_t servo_id, uint16_t current_deg);

/**
 * @brief  Check whether current angle has reached the lower limit
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg <= lower_limit_deg, 0 otherwise (or if invalid id)
 */
int Servo_IsAtLowerLimit(uint8_t servo_id, uint16_t current_deg);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
