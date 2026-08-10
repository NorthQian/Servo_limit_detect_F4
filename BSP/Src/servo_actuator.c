/**
 ******************************************************************************
 * @file    servo_actuator.c
 * @brief   Noetix Robotics servo actuator protocol driver
 * @note    Generates 129-byte servo-move command frames and sends them
 *          via USART3 (PB10, single-wire half-duplex TX, 115200 8N1).
 *
 *          Protocol reference: servo_actuator_noetix.py
 *
 *          Frame layout (128 bytes):
 *            [0-1]   Frame header:  0x55 0x55
 *            [2]     Payload length: 124
 *            [3]     Sequence number (auto-increment, wraps at 256)
 *            [4]     Lock flag (0=locked, 1=unlocked)
 *            [5]     Command type: 0x03 (SERVO_MOVE)
 *            [6]     Servo count (always 36)
 *            [7-8]   Movement time (big-endian, ms)
 *            [9-116] 36 servo groups × 3 bytes (ch, pulse_H, pulse_L)
 *            [117-126] Reserved (10 bytes of 0x00)
 *            [127]   Checksum (sum of bytes 0..126 mod 256)
 ******************************************************************************
 */

#include "servo_actuator.h"
#include "usart.h"

/* Module state ---------------------------------------------------------------*/
static uint8_t servo_seq = 0;   /* Rolling sequence number */

/**
 * @brief  Convert angle to pulse width
 * @param  angle_deg: servo angle in degrees
 * @return pulse width in us (500 ~ 2500)
 */
static inline uint16_t angle_to_pulse(float angle_deg)
{
    int32_t pulse = (int32_t)(500.0f + angle_deg * SERVO_ANGLE_SCALE + 0.5f);
    if (pulse < SERVO_PULSE_MIN) pulse = SERVO_PULSE_MIN;
    if (pulse > SERVO_PULSE_MAX) pulse = SERVO_PULSE_MAX;
    return (uint16_t)pulse;
}

/**
 * @brief  Generate and transmit a servo synchronous-move command frame
 * @param  channels: array of (channel_id, angle_deg) pairs
 * @param  num: number of entries in channels array (0 ~ 36)
 * @param  time_ms: movement time in milliseconds (0 ~ 65535)
 */
void Servo_SendAngles(ServoChannel_t *channels, uint8_t num, uint16_t time_ms)
{
    uint8_t  frame[SERVO_TOTAL_FRAME_LEN];
    uint16_t pulse_map[SERVO_NUM_CHANNELS];
    uint8_t  i;
    uint16_t sum;
    uint8_t  idx;

    /* Sanity checks */
    if (num > SERVO_NUM_CHANNELS) num = SERVO_NUM_CHANNELS;

    /* Build per-channel pulse map (default = 0, servo board ignores via PWM_LOW_THRE) */
    for (i = 0; i < SERVO_NUM_CHANNELS; i++) {
        pulse_map[i] = 0;   /* 0 = don't care, unmatched debug software behavior */
    }
    for (i = 0; i < num; i++) {
        if (channels[i].channel < SERVO_NUM_CHANNELS) {
            pulse_map[channels[i].channel] = angle_to_pulse(channels[i].angle);
        }
    }

    /* --- Assemble frame --- */
    idx = 0;

    /* Header */
    frame[idx++] = SERVO_FRAME_HEADER1;   /* 0x55 */
    frame[idx++] = SERVO_FRAME_HEADER2;   /* 0x55 */
    frame[idx++] = SERVO_PAYLOAD_LENGTH;  /* 125 */

    /* Seq + Lock + Cmd */
    frame[idx++] = servo_seq;
    servo_seq = (servo_seq + 1) % 256;
    frame[idx++] = 1;                     /* unlock */
    frame[idx++] = SERVO_CMD_SERVO_MOVE;  /* 0x03 */

    /* Servo count (always 36) + time (big-endian) */
    frame[idx++] = 36;  /* always transmit all 36 servos */
    frame[idx++] = (uint8_t)((time_ms >> 8) & 0xFF);
    frame[idx++] = (uint8_t)(time_ms & 0xFF);

    /* 36 groups: ch(1) + pulseH(1) + pulseL(1) */
    for (i = 0; i < SERVO_NUM_CHANNELS; i++) {
        frame[idx++] = i;
        frame[idx++] = (uint8_t)((pulse_map[i] >> 8) & 0xFF);
        frame[idx++] = (uint8_t)(pulse_map[i] & 0xFF);
    }

    /* Reserved (10 bytes) */
    for (i = 0; i < SERVO_RESERVED_LEN; i++) {
        frame[idx++] = 0x00;
    }

    /* Checksum: sum of all bytes from index 0 to idx-1, mod 256 */
    sum = 0;
    for (i = 0; i < idx; i++) {
        sum += frame[i];
    }
    frame[idx++] = (uint8_t)(sum & 0xFF);

    /* Transmit via USART2  full-duplex TX, 115200 */
//    HAL_UART_Transmit(&huart2, frame, 128, HAL_MAX_DELAY);
}

/**
 * @brief  Servo sweep test: cycle through channels 1-29
 * @note   0° → 180° → 0°, step 45° every 500ms
 *         TM1637 shows current channel number, LED toggles each step
 */
void Servo_SweepTest(void)
{
    static uint8_t  sweep_ch = 1;
    static uint8_t  sweep_dir = 0;      /* 0=forward, 1=reverse */
    static float    sweep_angle = 0.0f;
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();

//    TM1637_ShowNum(0, sweep_ch);

    if (now - last_tick >= 500) {
        last_tick = now;

        ServoChannel_t ch = { .channel = sweep_ch, .angle = sweep_angle };
        Servo_SendAngles(&ch, 1, 500);

        if (sweep_dir == 0) {
            sweep_angle += 45.0f;
            if (sweep_angle >= 180.0f) {
                sweep_angle = 180.0f;
                sweep_dir = 1;
            }
        } else {
            sweep_angle -= 45.0f;
            if (sweep_angle <= 0.0f) {
                sweep_angle = 0.0f;
                sweep_dir = 0;
                /* Finished one full cycle, move to next channel */
                sweep_ch++;
                if (sweep_ch > 29) sweep_ch = 1;
            }
        }
        LED_TOGGLE();
    }
}
