/**
 ******************************************************************************
 * @file    servo_calib.c
 * @brief   Servo stall-current calibration — sniff upper/lower limit angles
 * @note    Non-blocking state machine. Call Calib_Run() every ~50 ms.
 *
 *          Scan pattern per servo:
 *            center → step outward (+2 deg) → stall / 270° → upper limit
 *            return center → step inward (-5 deg) → stall / 0° → lower limit
 *            advance to next servo
 *
 *          Stall detection:
 *            Current must exceed per-servo threshold continuously for
 *            CALIB_STALL_HOLD_MS (500 ms) before a stall is confirmed.
 *            A CALIB_BLANK_MS (100 ms) window after each move suppresses
 *            acceleration current spikes.
 ******************************************************************************
 */

#include "servo_calib.h"
#include "servo.h"
#include "servo_actuator.h"
#include "current.h"
#include "main.h"
#include "bsp_log.h"

/* Module state ---------------------------------------------------------------*/
CalibState_t   calib_state   = CALIB_IDLE;
uint8_t        cur_idx       = 0;
uint16_t       cur_angle     = 0;
uint32_t       move_end_tick = 0;
uint32_t       blank_until   = 0;
uint32_t       stall_start   = 0;
uint8_t        stall_armed   = 0;

/* Servo calibration table ----------------------------------------------------*/
/*
 * Center angles are sourced from servo_config.h servo_param[].temp_deg.
 * Update here if servo_config.h changes.
 *
 *   idx  ch  center  name
 *   ---  --  ------  --------------------
 *    0    0    135   L_eyebrow_head_ver
 *    1    1    135   R_eyebrow_head_ver
 *    2    2    135   L_eyebrow_head_hor
 *    3    3    135   R_eyebrow_head_hor
 *    4    4    135   L_eyebrow_tail
 *    5    5    135   R_eyebrow_tail
 *    6    6    135   L_eyebrow_hor
 *    7    7    135   R_eyebrow_hor
 *    8    8    140   L_upper_eyelid
 *    9    9    125   R_upper_eyelid
 *   10   10    135   L_vertical_eyeball
 *   11   11    135   R_vertical_eyeball
 *   12   12    135   L_horizontal_eyeball
 *   13   13    135   R_horizontal_eyeball
 *   14   14    135   R_cheek
 *   15   15    135   L_cheek
 *   16   16    147   mid_upper_lip_ver
 *   17   17    190   mid_upper_lip_hor
 *   18   18    165   mid_lower_lip_hor
 *   19   19    155   mid_lower_lip_ver
 *   20   20    115   upper_lip
 *   21   21    105   lower_lip
 *   22   22    135   R_upper_mouth
 *   23   23    135   L_lower_mouth
 *   24   24    130   L_upper_mouth
 *   25   25    135   R_lower_mouth
 *   26   26     95   chin_vertical
 */
CalibEntry_t calib_results[CALIB_SERVO_COUNT] = {
    { .channel =  0, .center = 135, .thre_current = 0.8f },
    { .channel =  1, .center = 135, .thre_current = 0.8f },
    { .channel =  2, .center = 135, .thre_current = 0.8f },
    { .channel =  3, .center = 135, .thre_current = 0.8f },
    { .channel =  4, .center = 135, .thre_current = 0.8f },
    { .channel =  5, .center = 135, .thre_current = 0.8f },
    { .channel =  6, .center = 135, .thre_current = 0.8f },
    { .channel =  7, .center = 135, .thre_current = 0.8f },
    { .channel =  8, .center = 140, .thre_current = 0.8f },
    { .channel =  9, .center = 125, .thre_current = 0.8f },
    { .channel = 10, .center = 135, .thre_current = 0.8f },
    { .channel = 11, .center = 135, .thre_current = 0.8f },
    { .channel = 12, .center = 135, .thre_current = 0.8f },
    { .channel = 13, .center = 135, .thre_current = 0.8f },
    { .channel = 14, .center = 135, .thre_current = 0.8f },
    { .channel = 15, .center = 135, .thre_current = 0.8f },
    { .channel = 16, .center = 147, .thre_current = 0.8f },
    { .channel = 17, .center = 190, .thre_current = 0.8f },
    { .channel = 18, .center = 165, .thre_current = 0.8f },
    { .channel = 19, .center = 155, .thre_current = 0.8f },
    { .channel = 20, .center = 115, .thre_current = 0.8f },
    { .channel = 21, .center = 105, .thre_current = 0.8f },
    { .channel = 22, .center = 135, .thre_current = 0.8f },
    { .channel = 23, .center = 135, .thre_current = 0.8f },
    { .channel = 24, .center = 130, .thre_current = 0.8f },
    { .channel = 25, .center = 135, .thre_current = 0.8f },
    { .channel = 26, .center =  95, .thre_current = 0.8f },
};


/**
 * @brief  Print calibration report via SEGGER RTT
 */
void Calib_ExportReport(void)
{
    uint8_t i;

    PrintLog("\r\n");
    PrintLog(
        "============================================================\r\n");
    PrintLog(
        "  Servo Calibration Report\r\n");
    PrintLog(
        "------------------------------------------------------------\r\n");
    PrintLog(
        "  %-4s %-3s %-6s %-6s %-6s\r\n",
        "Name", "CH", "Center", "Lower", "Upper");
    PrintLog(
        "------------------------------------------------------------\r\n");

    for (i = 0; i < CALIB_SERVO_COUNT; i++) {
        CalibEntry_t *e = &calib_results[i];
        PrintLog(
            "  A%-3d  %02d   %-6d %-6d %-6d %s\r\n",
            i, e->channel, e->center, e->lower_limit, e->upper_limit,
            e->calib_done ? "" : "(skipped)");
    }

    PrintLog(
        "============================================================\r\n");
    PrintLog(
        "  Total: %d servos calibrated\r\n", CALIB_SERVO_COUNT);
    PrintLog(
        "============================================================\r\n\r\n");
}

/**
 * @brief  Print servo_limits[] table via SEGGER RTT
 * @note   Prints runtime-updated limits from calibration sweep
 */
void Calib_ExportServoLimits(void)
{
    uint8_t i;

    PrintLog("\r\n");
    PrintLog(
        "============================================================\r\n");
    PrintLog(
        "  Servo Limit Sweep Results (servo_limits[])\r\n");
    PrintLog(
        "------------------------------------------------------------\r\n");
    PrintLog(
        "  %-4s %-3s %-6s %-6s %-6s\r\n",
        "Idx", "CH", "Center", "Lower", "Upper");
    PrintLog(
        "------------------------------------------------------------\r\n");

    for (i = 0; i < SERVO_NUM; i++) {
        PrintLog(
            "  %-4d  %02d   %-6d %-6d %-6d\r\n",
            i,
            servo_limits[i].channel,
            servo_limits[i].center_deg,
            servo_limits[i].lower_limit_deg,
            servo_limits[i].upper_limit_deg);
    }

    PrintLog(
        "============================================================\r\n");
    PrintLog(
        "  Total: %d servos\r\n", SERVO_NUM);
    PrintLog(
        "============================================================\r\n\r\n");
}
