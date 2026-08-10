/**
 ******************************************************************************
 * @file    sweep.h
 * @brief   Servo sweep scanner — scans each servo through its limit range
 * @note    Non-blocking state machine. Call Sweep_Run() periodically.
 *          Scan pattern: center → upper_limit → lower_limit → center → next
 ******************************************************************************
 */

#ifndef __SWEEP_H
#define __SWEEP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Sweep state ---------------------------------------------------------------*/
typedef enum {
    SWEEP_IDLE = 0,       /* Not running                                    */
    SWEEP_TO_UPPER,       /* Moving step-by-step toward upper_limit_deg     */
    SWEEP_TO_LOWER,       /* Moving step-by-step toward lower_limit_deg     */
    SWEEP_TO_CENTER,      /* Returning step-by-step to center_deg           */
    SWEEP_NEXT,           /* Advance to next servo index                    */
    SWEEP_DONE            /* All servos scanned                             */
} SweepState_t;

/* API Functions -------------------------------------------------------------*/

/**
 * @brief  Configure sweep parameters
 * @param  step_deg: angle increment per step (degrees)
 * @param  interval_ms: delay between steps (milliseconds)
 * @param  move_time_ms: servo movement time per step (milliseconds)
 */
void Sweep_Init(uint16_t step_deg, uint16_t interval_ms, uint16_t move_time_ms);

/**
 * @brief  Start scanning from servo index 0
 * @note   Sets state to SWEEP_TO_UPPER, first target = center_deg of servo 0
 */
void Sweep_Start(void);

/**
 * @brief  Stop scanning immediately
 * @note   Returns to SWEEP_IDLE, servo holds last position
 */
void Sweep_Stop(void);

/**
 * @brief  Run one step of the sweep state machine
 * @note   Call this periodically (e.g. from a FreeRTOS task or main loop).
 *         Uses HAL_GetTick() internally for non-blocking timing.
 */
void Sweep_Run(void);

/**
 * @brief  Check if sweep is currently running
 * @return 1 if running, 0 if idle or done
 */
uint8_t Sweep_IsRunning(void);

/**
 * @brief  Get current servo index being scanned
 * @return servo index (0 ~ SERVO_NUM-1)
 */
uint8_t Sweep_CurrentServo(void);

#ifdef __cplusplus
}
#endif

#endif /* __SWEEP_H */
