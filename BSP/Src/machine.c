/**
 ******************************************************************************
 * @file    machine.c
 * @brief   Machine profile abstraction — switchable servo configurations.
 * @note    The runtime tables servo_limits[] (servo.c) and calib_results[]
 *          (servo_calib.c) are rebuilt from the active profile's params
 *          whenever the machine is selected (boot default or LCD switch).
 ******************************************************************************
 */

#include <stddef.h>         /* NULL */
#include "machine.h"
#include "servo_config.h"   /* parameter tables */
#include "servo.h"          /* ServoLimit_t, servo_limits[] */
#include "servo_calib.h"    /* CalibEntry_t, calib_results[], CalibState_t */

/* Sweep state, defined in servo_calib.c --------------------------------------*/
extern uint8_t      cur_idx;
extern uint16_t     cur_angle;
extern CalibState_t calib_state;

/* Default profile on boot (index into machine_profiles[]) ---------------------
 * 0 = XHEAD (27ch, no skip), 1 = XHEADLITE (27ch, skip 10), 2 = V3.0 (29ch)   */
#define DEFAULT_PROFILE 1

/* XHEADLITE skips these 10 servos (originally servo_calib.c skipped_servos)  */
static const uint8_t skip_xheadlite[] = {2, 3, 4, 5, 6, 7, 14, 15, 20, 21};

/* Machine profiles ------------------------------------------------------------*/
const MachineProfile_t machine_profiles[MACHINE_PROFILE_COUNT] = {
    { "XHEAD",     MACHINE_XHEAD_SERVO_NUM,     servo_param_xhead,  NULL,           0  },
    { "XHEADLITE", MACHINE_XHEADLITE_SERVO_NUM, servo_param_xhead,  skip_xheadlite, 10 },
    { "V3.0",      MACHINE_V3_SERVO_NUM,        servo_param_v3,     NULL,           0  },
};

static uint8_t g_machine_idx = 0;

/**
 * @brief  Rebuild servo_limits[] and calib_results[] from a profile's params
 */
static void machine_refill(uint8_t idx)
{
    const MachineProfile_t *mp = &machine_profiles[idx];
    uint8_t i;

    for (i = 0; i < mp->servo_num; i++) {
        const ServoParam_t *p = &mp->params[i];
        uint16_t hi = (p->start_deg > p->end_deg) ? p->start_deg : p->end_deg;
        uint16_t lo = (p->start_deg < p->end_deg) ? p->start_deg : p->end_deg;

        servo_limits[i].channel         = p->channel;
        servo_limits[i].center_deg      = p->temp_deg;
        servo_limits[i].start_deg       = p->start_deg;
        servo_limits[i].end_deg         = p->end_deg;
        servo_limits[i].upper_limit_deg = hi;
        servo_limits[i].lower_limit_deg = lo;

        calib_results[i].channel      = p->channel;
        calib_results[i].center       = p->temp_deg;
        calib_results[i].thre_current = 0.0f;   /* 0 = use default threshold */
        calib_results[i].upper_limit  = 0;
        calib_results[i].lower_limit  = 0;
        calib_results[i].upper_trim   = 0;
        calib_results[i].lower_trim   = 0;
        calib_results[i].calib_done   = 0;
    }
}

void Machine_Init(void)
{
    g_machine_idx = DEFAULT_PROFILE;
    machine_refill(g_machine_idx);
}

uint8_t Machine_GetServoNum(void)
{
    return machine_profiles[g_machine_idx].servo_num;
}

uint8_t Machine_GetProfileCount(void)
{
    return MACHINE_PROFILE_COUNT;
}

const MachineProfile_t *Machine_GetCurrent(void)
{
    return &machine_profiles[g_machine_idx];
}

const ServoParam_t *Machine_GetServoParam(uint8_t idx)
{
    const MachineProfile_t *mp = Machine_GetCurrent();
    if (idx >= mp->servo_num) {
        return NULL;
    }
    return &mp->params[idx];
}

uint8_t Machine_IsServoSkipped(uint8_t idx)
{
    const MachineProfile_t *mp = Machine_GetCurrent();
    uint8_t i;

    for (i = 0; i < mp->skip_count; i++) {
        if (mp->skip_list[i] == idx) return 1;
    }
    return 0;
}

/**
 * @brief  Switch active machine (refuses while a sweep is running)
 * @param  idx: profile index (0 = XHEAD, 1 = XHEADLITE, 2 = V3.0)
 * @return 0 on success, -1 if calibrating (not allowed), -2 if bad index
 */
int Machine_Switch(uint8_t idx)
{
    if (idx >= MACHINE_PROFILE_COUNT) {
        return -2;
    }
    /* Only allowed when idle (not started) or done (finished) */
    if (calib_state != CALIB_IDLE && calib_state != CALIB_DONE) {
        return -1;
    }

    g_machine_idx = idx;
    Calib_ResetSweep();   /* reset cur_idx / cur_angle / calib_state / report flag */
    machine_refill(idx);
    return 0;
}
