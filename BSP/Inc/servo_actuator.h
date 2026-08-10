/**
 ******************************************************************************
 * @file    servo_actuator.h
 * @brief   Noetix Robotics servo actuator protocol driver (TX-only via USART3)
 * @note    Protocol: 129-byte frame, 115200 bps, 8N1
 *          Frame: 0x55 0x55 | Len(125) | Seq | Lock | Cmd(0x03) | Num |
 *                 Time(2B) | 36×(Ch+PulseH+PulseL) | Reserved(10B) | CRC
 *          PB10 --> USART3_TX (single-wire half-duplex)
 ******************************************************************************
 */

#ifndef __SERVO_ACTUATOR_H
#define __SERVO_ACTUATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Protocol constants ---------------------------------------------------------*/
#define SERVO_FRAME_HEADER1      0x55
#define SERVO_FRAME_HEADER2      0x55
#define SERVO_PAYLOAD_LENGTH     124
#define SERVO_CMD_SERVO_MOVE     0x03
#define SERVO_TOTAL_FRAME_LEN    128
#define SERVO_NUM_CHANNELS       36
#define SERVO_RESERVED_LEN       10

#define SERVO_PULSE_MIN          500
#define SERVO_PULSE_MAX          2500
#define SERVO_ANGLE_SCALE        7.407f   /* (2500-500)/270 */

/* Servo channel descriptor ---------------------------------------------------*/
typedef struct {
    uint8_t channel;     /* Channel ID: 0 ~ 35 */
    float   angle;       /* Target angle in degrees, typical range 0 ~ 270 */
} ServoChannel_t;

/* API Functions --------------------------------------------------------------*/

/**
 * @brief  Generate and transmit a servo synchronous-move command frame
 * @param  channels: array of (channel_id, angle_deg) pairs
 * @param  num: number of entries in channels array (max 36)
 * @param  time_ms: movement time in milliseconds (0 ~ 65535)
 * @note   Sends the frame via USART3 (PB10) at 115200 bps
 */
void Servo_SendAngles(ServoChannel_t *channels, uint8_t num, uint16_t time_ms);
void Servo_SweepTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_ACTUATOR_H */
