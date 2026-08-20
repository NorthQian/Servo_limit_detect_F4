/**
 ******************************************************************************
 * @file    machine.h
 * @brief   Machine profile abstraction — switchable servo configs
 * @note    Supported machines: XHEAD (27ch), XHEADLITE (27ch, skip 10),
 *          V3.0 (29ch). Runtime tables (servo_limits[] / calib_results[])
 *          are derived from the active profile at startup and on switch.
 ******************************************************************************
 */

#ifndef __MACHINE_H
#define __MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Single servo parameter descriptor -------------------------------------------
 * Simplified: array index is the servo id (A0..A28), so only the hardware
 * channel number is kept. name/info/coefficient were unused by any code.    */
typedef struct {
    uint8_t  channel;       /* Hardware channel index 0 ~ 35          */
    uint16_t start_deg;     /* Start-limit angle (direction-aware)    */
    uint16_t end_deg;       /* End-limit angle                        */
    uint16_t temp_deg;      /* Default / rest (center) angle          */
} ServoParam_t;

/* Runtime tables use this size = protocol channel cap; adding a new
 * machine never requires resizing the arrays.                             */
#define MACHINE_MAX_SERVOS  36

/* Machine profile ------------------------------------------------------------*/
typedef struct {
    const char        *name;       /* "XHEAD" / "XHEADLITE" / "V3.0"   */
    uint8_t            servo_num;  /* 27 / 27 / 29                     */
    const ServoParam_t *params;    /* Parameter table for this machine */
    const uint8_t      *skip_list; /* Skipped indices, NULL = none     */
    uint8_t            skip_count;
} MachineProfile_t;

/* Defined in machine.c (incomplete array on purpose) */
extern const MachineProfile_t machine_profiles[];
/* Fixed constant — do NOT use sizeof() on the extern incomplete array */
#define MACHINE_PROFILE_COUNT 3

/* API -------------------------------------------------------------------------*/

void Machine_Init(void);                /* Default machine + refill runtime tables */
uint8_t Machine_GetServoNum(void);      /* Active servo count (replaces SERVO_NUM) */
uint8_t Machine_GetProfileCount(void);
const MachineProfile_t *Machine_GetCurrent(void);
const ServoParam_t *Machine_GetServoParam(uint8_t idx);
uint8_t Machine_IsServoSkipped(uint8_t idx);
int Machine_Switch(uint8_t idx);        /* 0=ok, -1=calibrating, -2=bad index */

#ifdef __cplusplus
}
#endif

#endif /* __MACHINE_H */
