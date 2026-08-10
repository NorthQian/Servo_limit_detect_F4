/**
 ******************************************************************************
 * @file    sweep.c
 * @brief   Servo sweep scanner implementation
 * @note    Non-blocking state machine driven by HAL_GetTick().
 *          Scan pattern per servo:
 *            center_deg → upper_limit_deg → lower_limit_deg → center_deg
 *          Then advances to the next servo in servo_limits[].
 ******************************************************************************
 */

#include "sweep.h"
#include "servo.h"
#include "servo_actuator.h"
#include "main.h"

/* Module state ---------------------------------------------------------------*/
static SweepState_t sweep_state   = SWEEP_IDLE;
static uint8_t      sweep_idx     = 0;
static float        target_angle  = 0.0f;
static uint32_t     last_tick     = 0;
static uint16_t     step          = 45;
static uint16_t     interval      = 500;
static uint16_t     move_time     = 500;
static uint8_t      running       = 0;

/* --- Helper: send single-channel angle command --- */
static void sweep_send(uint8_t ch, float angle)
{
    ServoChannel_t sc;
    sc.channel = ch;
    sc.angle   = angle;
    Servo_SendAngles(&sc, 1, move_time);
}

/* API Functions --------------------------------------------------------------*/

/**
 * @brief  Configure sweep parameters
 * @param  step_deg: angle increment per step (degrees)
 * @param  interval_ms: delay between steps (milliseconds)
 * @param  move_time_ms: servo movement time per step (milliseconds)
 */
void Sweep_Init(uint16_t step_deg, uint16_t interval_ms, uint16_t move_time_ms)
{
    step      = step_deg;
    interval  = interval_ms;
    move_time = move_time_ms;
}

/**
 * @brief  Start scanning from servo index 0
 */
void Sweep_Start(void)
{
    if (running) {
        return;
    }
    sweep_idx    = 0;
    target_angle = (float)servo_limits[0].center_deg;
    sweep_state  = SWEEP_TO_UPPER;
    last_tick    = HAL_GetTick();
    running      = 1;

    /* Send initial position: move to center of first servo */
    sweep_send(servo_limits[0].channel, target_angle);
}

/**
 * @brief  Stop scanning immediately
 */
void Sweep_Stop(void)
{
    sweep_state = SWEEP_IDLE;
    running     = 0;
}

/**
 * @brief  Run one step of the sweep state machine
 */
void Sweep_Run(void)
{
    const ServoLimit_t *lim;
    uint32_t now;
    uint16_t upper;
    uint16_t lower;
    uint16_t center;

    if (!running) {
        return;
    }

    now = HAL_GetTick();
    if ((now - last_tick) < interval) {
        return;
    }
    last_tick = now;

    lim    = &servo_limits[sweep_idx];
    upper  = lim->upper_limit_deg;
    lower  = lim->lower_limit_deg;
    center = lim->center_deg;

    switch (sweep_state) {

    case SWEEP_TO_UPPER:
        target_angle += (float)step;
        if (target_angle >= (float)upper) {
            target_angle = (float)upper;
            sweep_state = SWEEP_TO_LOWER;
        }
        sweep_send(lim->channel, target_angle);
        break;

    case SWEEP_TO_LOWER:
        target_angle -= (float)step;
        if (target_angle <= (float)lower) {
            target_angle = (float)lower;
            sweep_state = SWEEP_TO_CENTER;
        }
        sweep_send(lim->channel, target_angle);
        break;

    case SWEEP_TO_CENTER:
        target_angle += (float)step;
        if (target_angle >= (float)center) {
            target_angle = (float)center;
            sweep_state = SWEEP_NEXT;
        }
        sweep_send(lim->channel, target_angle);
        break;

    case SWEEP_NEXT:
        sweep_idx++;
        if (sweep_idx < SERVO_NUM) {
            lim         = &servo_limits[sweep_idx];
            target_angle = (float)lim->center_deg;
            sweep_state  = SWEEP_TO_UPPER;
            sweep_send(lim->channel, target_angle);
        } else {
            sweep_state = SWEEP_DONE;
            running     = 0;
        }
        break;

    case SWEEP_DONE:
    case SWEEP_IDLE:
    default:
        running = 0;
        break;
    }
}

/**
 * @brief  Check if sweep is currently running
 * @return 1 if running, 0 if idle or done
 */
uint8_t Sweep_IsRunning(void)
{
    return running;
}

/**
 * @brief  Get current servo index being scanned
 * @return servo index (0 ~ SERVO_NUM-1)
 */
uint8_t Sweep_CurrentServo(void)
{
    return sweep_idx;
}