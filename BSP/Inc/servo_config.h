/**
 ******************************************************************************
 * @file    servo_config.h
 * @brief   Servo parameter configuration — generated from servo_config yaml
 * @note    Multi-machine parameter tables. Array index is the servo id
 *          (A0..A28); each entry is {channel, start_deg, end_deg, temp_deg}.
 *          XHEAD and XHEADLITE share servo_param_xhead (27ch), differing
 *          only in the per-machine skip list (see machine.c).
 *          V3.0 table from ref_29_servo_config.yaml (29ch).
 ******************************************************************************
 */

#ifndef __SERVO_CONFIG_H
#define __SERVO_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "machine.h"     /* ServoParam_t */

/* Number of servos per machine ----------------------------------------------*/
#define MACHINE_XHEAD_SERVO_NUM     27
#define MACHINE_XHEADLITE_SERVO_NUM 27
#define MACHINE_V3_SERVO_NUM        29

/* XHEAD / XHEADLITE parameter table (27 servos, A0 ~ A26) -------------------*/
static const ServoParam_t servo_param_xhead[MACHINE_XHEAD_SERVO_NUM] = {
    /*  ch  start  end  temp   name                              */
    {  0,   220,   70,  135 },  /* A0  L_eyebrow_head_ver        */
    {  1,    50,  200,  135 },  /* A1  R_eyebrow_head_ver        */
    {  2,   105,  205,  135 },  /* A2  L_eyebrow_head_hor        */
    {  3,   165,   65,  135 },  /* A3  R_eyebrow_head_hor        */
    {  4,    85,  135,  135 },  /* A4  L_eyebrow_tail            */
    {  5,   185,  135,  135 },  /* A5  R_eyebrow_tail            */
    {  6,   170,  100,  135 },  /* A6  L_eyebrow_hor             */
    {  7,   100,  170,  135 },  /* A7  R_eyebrow_hor             */
    {  8,   120,  230,  140 },  /* A8  L_upper_eyelid            */
    {  9,   145,   35,  125 },  /* A9  R_upper_eyelid            */
    { 10,   160,  105,  135 },  /* A10 L_vertical_eyeball        */
    { 11,   110,  165,  135 },  /* A11 R_vertical_eyeball        */
    { 12,   150,  115,  135 },  /* A12 L_horizontal_eyeball      */
    { 13,   120,  155,  135 },  /* A13 R_horizontal_eyeball      */
    { 14,    95,  135,  135 },  /* A14 R_cheek                   */
    { 15,   175,  135,  135 },  /* A15 L_cheek                   */
    { 16,    95,  190,  147 },  /* A16 mid_upper_lip_ver         */
    { 17,   160,  200,  190 },  /* A17 mid_upper_lip_hor         */
    { 18,   185,  155,  165 },  /* A18 mid_lower_lip_hor         */
    { 19,   200,  100,  155 },  /* A19 mid_lower_lip_ver         */
    { 20,   160,   70,  115 },  /* A20 upper_lip                 */
    { 21,    60,  160,  105 },  /* A21 lower_lip                 */
    { 22,   160,  100,  135 },  /* A22 R_upper_mouth             */
    { 23,   180,   90,  135 },  /* A23 L_lower_mouth             */
    { 24,   105,  165,  130 },  /* A24 L_upper_mouth             */
    { 25,    90,  180,  135 },  /* A25 R_lower_mouth             */
    { 26,    85,  150,   95 },  /* A26 chin_vertical             */
};

/* V3.0 parameter table (29 servos, A0 ~ A28) from ref_29_servo_config.yaml -*/
static const ServoParam_t servo_param_v3[MACHINE_V3_SERVO_NUM] = {
    /*  ch  start  end  temp   name                            */
    {  0,   125,  170,  150 },  /* A0  L_eyebrow_head_ver      */
    {  1,   145,  100,  120 },  /* A1  R_eyebrow_head_ver      */
    {  2,    95,  135,  135 },  /* A2  L_eyebrow_head_hor      */
    {  3,   175,  135,  135 },  /* A3  R_eyebrow_head_hor      */
    {  4,    70,  135,  102 },  /* A4  L_eyebrow_tail          */
    {  5,   200,  135,  168 },  /* A5  R_eyebrow_tail          */
    {  6,   125,  185,  135 },  /* A6  L_upper_eyelid          */
    {  7,   140,   80,  130 },  /* A7  R_upper_eyelid          */
    {  8,    95,  160,  125 },  /* A8  L_vertical_eyeball      */
    {  9,   175,  110,  145 },  /* A9  R_vertical_eyeball      */
    { 10,   100,  175,  145 },  /* A10 L_horizontal_eyeball    */
    { 11,   160,   85,  115 },  /* A11 R_horizontal_eyeball    */
    { 12,   135,  120,  135 },  /* A12 L_cheek                */
    { 13,   135,  150,  135 },  /* A13 R_cheek                */
    { 14,   140,  165,  165 },  /* A14 nose                   */
    { 15,    75,  115,   90 },  /* A15 mid_upper_lip_hor      */
    { 16,   125,  210,  165 },  /* A16 mid_upper_lip_ver      */
    { 17,   210,  150,  190 },  /* A17 mid_lower_lip_hor      */
    { 18,   165,   90,  135 },  /* A18 mid_lower_lip_ver      */
    { 19,    40,  120,   85 },  /* A19 L_upper_lip            */
    { 20,   230,  150,  185 },  /* A20 R_upper_lip            */
    { 21,   180,   80,  140 },  /* A21 L_lower_lip            */
    { 22,    90,  190,  130 },  /* A22 R_lower_lip            */
    { 23,   100,  185,  135 },  /* A23 L_upper_mouth          */
    { 24,   215,  130,  165 },  /* A24 L_lower_mouth          */
    { 25,   170,   85,  135 },  /* A25 R_upper_mouth          */
    { 26,    50,  135,  100 },  /* A26 R_lower_mouth          */
    { 27,   135,  195,  145 },  /* A27 chin_vertical          */
    { 28,   130,  170,  150 },  /* A28 chin_horizontal        */
};

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_CONFIG_H */
