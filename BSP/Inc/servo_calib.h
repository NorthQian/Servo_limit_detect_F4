/**
 ******************************************************************************
 * @file    servo_calib.h
 * @brief   Servo stall-current calibration module — sniff upper/lower limits
 * @note    Drives each servo from center outward until stall (current > threshold)
 *          or hard-stop (270°), then inward to find the lower limit.
 *          Non-blocking state machine; call Calib_Run() periodically.
 *          Integrate into a FreeRTOS task (e.g. App_sweep_task) at ~50 ms.
 ******************************************************************************
 */

#ifndef __SERVO_CALIB_H
#define __SERVO_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================================
 *  Hardware configuration
 *    0 = high (full 27 servos)
 *    1 = low  (skip eyebrow_hor, eyebrow_tail, cheek, lip: 10 servos)
 * ========================================================================== */
#define HW_CONFIG_LOW           1

/* Calibration constants ------------------------------------------------------*/
#define CALIB_SERVO_COUNT       27       /* Total servos (A0 ~ A26)           */
#define CALIB_MAX_IDX           26       /* Last valid index                  */
#define CALIB_STEP_UP_DEG       2        /* Outward search step per iteration */
#define CALIB_STEP_DOWN_DEG     5        /* Inward search step (asymmetric)   */
#define CALIB_MOVE_MS           200      /* Servo movement time per step      */
#define CALIB_SETTLE_MS         200      /* Settle wait after each move       */
#define CALIB_HARD_MIN          0        /* Absolute mechanical minimum       */
#define CALIB_HARD_MAX          270      /* Absolute mechanical maximum       */
#define CALIB_STALL_CUR_A       0.8f     /* Default stall current threshold   */
#define CALIB_STALL_HOLD_MS     500      /* Debounce: must hold above thresh  */
#define CALIB_BLANK_MS          100      /* Ignore first N ms after move      */

/* Per-servo calibration entry ------------------------------------------------*/
typedef struct {
    uint8_t  channel;           /* Hardware channel index (0 ~ 26)            */
    uint16_t center;            /* [user set] Center / resting angle          */
    float    thre_current;      /* [user set] Stall threshold (A), 0 = default*/
    uint16_t upper_limit;       /* [result]   Discovered upper stall limit    */
    uint16_t lower_limit;       /* [result]   Discovered lower stall limit    */
    int16_t  upper_trim;        /* [user set] Fine-tune offset for upper      */
    int16_t  lower_trim;        /* [user set] Fine-tune offset for lower      */
    uint8_t  calib_done;        /* 1 = this servo has been calibrated         */
} CalibEntry_t;

/* Calibration state machine states -------------------------------------------*/
typedef enum {
    CALIB_IDLE = 0,             /* Not running                                */
    CALIB_SEARCH_UP,            /* Stepping outward from center → upper limit */
    CALIB_TRANSITION,           /* Return to center, arm lower search         */
    CALIB_SEARCH_DOWN,          /* Stepping inward from center → lower limit  */
    CALIB_NEXT_SERVO,           /* Advance to next servo                      */
    CALIB_DONE                  /* All servos calibrated                      */
} CalibState_t;

/* Calibration results array (defined in servo_calib.c) ----------------------*/
extern CalibEntry_t calib_results[CALIB_SERVO_COUNT];

/* API Functions --------------------------------------------------------------*/


/**
 * @brief  Print calibration report via SEGGER RTT
 * @note   Outputs channel, center, lower, upper for all calibrated servos
 */
void Calib_ExportReport(void);

/**
 * @brief  Print servo_limits[] table via SEGGER RTT
 * @note   Prints runtime-updated limits (from calibration), not static calib_results[]
 */
void Calib_ExportServoLimits(void);

/**
 * @brief  已完成且未跳过的舵机数（进度条用）
 * @return 0 ~ 扣除跳过后的总数
 */
uint8_t Calib_GetSweptCount(void);

/**
 * @brief  总扫频舵机数（已扣除跳过的舵机）
 * @return HW_CONFIG_LOW 下为 CALIB_SERVO_COUNT - SKIP_COUNT
 */
uint8_t Calib_GetTotalSweptCount(void);

#if HW_CONFIG_LOW
/**
 * @brief  Check if a servo index should be skipped on low-config hardware
 * @param  idx: servo index (0 ~ 26)
 * @return 1 if servo should be skipped, 0 otherwise
 */
uint8_t IsServoSkipped(uint8_t idx);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_CALIB_H */
