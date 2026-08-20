/**
 ******************************************************************************
 * @file    servo.c
 * @brief   Servo limit detection module — runtime table from active machine
 * @note    servo_limits[] is refilled from the active machine profile by
 *          machine_refill() (machine.c) at startup and on machine switch.
 ******************************************************************************
 */

#include "servo.h"
#include "machine.h"

/* Servo limit table (runtime, refilled per active machine) -------------------*/
ServoLimit_t servo_limits[MACHINE_MAX_SERVOS];

/* API Functions --------------------------------------------------------------*/

/**
 * @brief  Get servo limit instance by servo ID
 * @param  servo_id: servo index (0 ~ active servo count-1)
 * @return pointer to ServoLimit_t, or NULL if out of range
 */
const ServoLimit_t *Servo_GetLimit(uint8_t servo_id)
{
    if (servo_id >= Machine_GetServoNum()) {
        return 0;
    }
    return &servo_limits[servo_id];
}

/**
 * @brief  Check whether current angle has reached the upper limit
 * @param  servo_id: servo index (0 ~ active servo count-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg >= upper_limit_deg, 0 otherwise
 */
int Servo_IsAtUpperLimit(uint8_t servo_id, uint16_t current_deg)
{
    if (servo_id >= Machine_GetServoNum()) {
        return 0;
    }
    return (current_deg >= servo_limits[servo_id].upper_limit_deg) ? 1 : 0;
}

/**
 * @brief  Check whether current angle has reached the lower limit
 * @param  servo_id: servo index (0 ~ active servo count-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg <= lower_limit_deg, 0 otherwise
 */
int Servo_IsAtLowerLimit(uint8_t servo_id, uint16_t current_deg)
{
    if (servo_id >= Machine_GetServoNum()) {
        return 0;
    }
    return (current_deg <= servo_limits[servo_id].lower_limit_deg) ? 1 : 0;
}