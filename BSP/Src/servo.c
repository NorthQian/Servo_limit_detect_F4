/**
 ******************************************************************************
 * @file    servo.c
 * @brief   Servo limit detection module — compile-time initialized limit table
 * @note    Each entry copies temp_deg/start_deg/end_deg from servo_config.h
 *          and pre-computes upper_limit_deg / lower_limit_deg.
 ******************************************************************************
 */

#include "servo.h"

/* Servo limit table ----------------------------------------------------------*/
/*  ch  center  start   end  upper  lower                                       */
ServoLimit_t servo_limits[SERVO_NUM] = {
    {  0,    135,    220,   70,   220,   70 },   /* A0  L_eyebrow_head_ver     */
    {  1,    135,     50,  200,   200,   50 },   /* A1  R_eyebrow_head_ver     */
    {  2,    135,    105,  205,   205,  105 },   /* A2  L_eyebrow_head_hor     */
    {  3,    135,    165,   65,   165,   65 },   /* A3  R_eyebrow_head_hor     */
    {  4,    135,     85,  135,   135,   85 },   /* A4  L_eyebrow_tail         */
    {  5,    135,    185,  135,   185,  135 },   /* A5  R_eyebrow_tail         */
    {  6,    135,    170,  100,   170,  100 },   /* A6  L_eyebrow_hor          */
    {  7,    135,    100,  170,   170,  100 },   /* A7  R_eyebrow_hor          */
    {  8,    140,    120,  230,   230,  120 },   /* A8  L_upper_eyelid         */
    {  9,    125,    145,   35,   145,   35 },   /* A9  R_upper_eyelid         */
    { 10,    135,    160,  105,   160,  105 },   /* A10 L_vertical_eyeball      */
    { 11,    135,    110,  165,   165,  110 },   /* A11 R_vertical_eyeball      */
    { 12,    135,    150,  115,   150,  115 },   /* A12 L_horizontal_eyeball    */
    { 13,    135,    120,  155,   155,  120 },   /* A13 R_horizontal_eyeball    */
    { 14,    135,     95,  135,   135,   95 },   /* A14 R_cheek                */
    { 15,    135,    175,  135,   175,  135 },   /* A15 L_cheek                */
    { 16,    147,     95,  190,   190,   95 },   /* A16 mid_upper_lip_ver      */
    { 17,    190,    160,  200,   200,  160 },   /* A17 mid_upper_lip_hor      */
    { 18,    165,    185,  155,   185,  155 },   /* A18 mid_lower_lip_hor      */
    { 19,    155,    200,  100,   200,  100 },   /* A19 mid_lower_lip_ver      */
    { 20,    115,    160,   70,   160,   70 },   /* A20 upper_lip              */
    { 21,    105,     60,  160,   160,   60 },   /* A21 lower_lip              */
    { 22,    135,    160,  100,   160,  100 },   /* A22 R_upper_mouth          */
    { 23,    135,    180,   90,   180,   90 },   /* A23 L_lower_mouth          */
    { 24,    130,    105,  165,   165,  105 },   /* A24 L_upper_mouth          */
    { 25,    135,     90,  180,   180,   90 },   /* A25 R_lower_mouth          */
    { 26,     95,     85,  150,   150,   85 },   /* A26 chin_vertical          */
};

/* API Functions --------------------------------------------------------------*/

/**
 * @brief  Get servo limit instance by servo ID
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1)
 * @return pointer to ServoLimit_t, or NULL if out of range
 */
const ServoLimit_t *Servo_GetLimit(uint8_t servo_id)
{
    if (servo_id >= SERVO_NUM) {
        return 0;
    }
    return &servo_limits[servo_id];
}

/**
 * @brief  Check whether current angle has reached the upper limit
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg >= upper_limit_deg, 0 otherwise
 */
int Servo_IsAtUpperLimit(uint8_t servo_id, uint16_t current_deg)
{
    if (servo_id >= SERVO_NUM) {
        return 0;
    }
    return (current_deg >= servo_limits[servo_id].upper_limit_deg) ? 1 : 0;
}

/**
 * @brief  Check whether current angle has reached the lower limit
 * @param  servo_id: servo index (0 ~ SERVO_NUM-1)
 * @param  current_deg: current angle in degrees
 * @return 1 if current_deg <= lower_limit_deg, 0 otherwise
 */
int Servo_IsAtLowerLimit(uint8_t servo_id, uint16_t current_deg)
{
    if (servo_id >= SERVO_NUM) {
        return 0;
    }
    return (current_deg <= servo_limits[servo_id].lower_limit_deg) ? 1 : 0;
}