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
#include "machine.h"

/* Module state ---------------------------------------------------------------*/
CalibState_t   calib_state   = CALIB_IDLE;
uint8_t        cur_idx       = 0;
uint16_t       cur_angle     = 0;
uint8_t        calib_report_pending = 0;   /* 1 = DONE report already printed */
uint32_t       move_end_tick = 0;
uint32_t       blank_until   = 0;
uint32_t       stall_start   = 0;
uint8_t        stall_armed   = 0;

/* Servo calibration table ----------------------------------------------------*/
/* Refilled at boot and on machine switch by machine_refill() (machine.c) from
   the active profile's params. Values here are the pre-switch defaults.       */
CalibEntry_t calib_results[MACHINE_MAX_SERVOS];


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

    for (i = 0; i < Machine_GetServoNum(); i++) {
        CalibEntry_t *e = &calib_results[i];
        PrintLog(
            "  A%-3d  %02d   %-6d %-6d %-6d %s\r\n",
            i, e->channel, e->center, e->lower_limit, e->upper_limit,
            e->calib_done ? "" : "(skipped)");
    }

    PrintLog(
        "============================================================\r\n");
    PrintLog(
        "  Total: %d servos calibrated\r\n", Machine_GetServoNum());
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

    for (i = 0; i < Machine_GetServoNum(); i++) {
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
        "  Total: %d servos\r\n", Machine_GetServoNum());
    PrintLog(
        "============================================================\r\n\r\n");
}

/**
 * @brief  已完成且未跳过的舵机数（进度条用）
 * @note   扫到 CALIB_DONE 时 cur_idx 已达当前机型舵机数，返回扣除跳过后的总数
 */
uint8_t Calib_GetSweptCount(void)
{
    uint8_t i, n = 0;
    for (i = 0; i < cur_idx && i < Machine_GetServoNum(); i++) {
        if (Machine_IsServoSkipped(i)) continue;
        n++;
    }
    return n;
}

/**
 * @brief  总扫频舵机数（已扣除当前机型跳过的舵机）
 * @note   XHEADLITE 返回 27 - 10 = 17；XHEAD 27；V3.0 29
 */
uint8_t Calib_GetTotalSweptCount(void)
{
    uint8_t i, n = 0;
    for (i = 0; i < Machine_GetServoNum(); i++) {
        if (Machine_IsServoSkipped(i)) continue;
        n++;
    }
    return n;
}

/**
 * @brief  Reset sweep state (calib_state/cur_idx/cur_angle/report flag)
 * @note   Called by Machine_Switch() when the active machine changes.
 */
void Calib_ResetSweep(void)
{
    calib_state         = CALIB_IDLE;
    cur_idx             = 0;
    cur_angle           = 0;
    calib_report_pending = 0;
}
