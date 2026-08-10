/**
 ******************************************************************************
 * @file    servo_config.h
 * @brief   Servo parameter configuration — generated from servo_config.yaml
 * @note    27 servos (A0 ~ A26) for XH1U robot face
 ******************************************************************************
 */

#ifndef __SERVO_CONFIG_H
#define __SERVO_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Number of servos -----------------------------------------------------------*/
#define SERVO_NUM   27

/* Single servo parameter descriptor ------------------------------------------*/
typedef struct {
    uint8_t  id;            /* Servo ID: 0 ~ 26 (A0 ~ A26)            */
    uint8_t  channel;       /* Hardware channel index: 0 ~ 26         */
    uint16_t start_deg;     /* Start-limit angle (degrees)            */
    uint16_t end_deg;       /* End-limit angle (degrees)              */
    uint16_t temp_deg;      /* Default / rest angle (degrees)         */
    uint8_t  coefficient;   /* Direction coefficient (1 = normal)     */
    char     name[32];      /* English identifier, e.g. "A0"          */
    char     info[48];      /* Description: EN_name CN_desc direction */
} ServoParam_t;

/* Servo parameter table ------------------------------------------------------*/
static const ServoParam_t servo_param[SERVO_NUM] = {
    /*  id  ch  start  end  temp  coeff  name   info                                */
    {   0,  0,   220,   70,  135,  1,    "A0",  "L_eyebrow_head_ver 左眉头 上——下"     },
    {   1,  1,    50,  200,  135,  1,    "A1",  "R_eyebrow_head_ver 右眉头 上——下"     },
    {   2,  2,   105,  205,  135,  1,    "A2",  "L_eyebrow_head_hor 左眉头 外——内"     },
    {   3,  3,   165,   65,  135,  1,    "A3",  "R_eyebrow_head_hor 右眉头 外——内"     },
    {   4,  4,    85,  135,  135,  1,    "A4",  "L_eyebrow_tail 左眉尾 上——下"         },
    {   5,  5,   185,  135,  135,  1,    "A5",  "R_eyebrow_tail 右眉尾 上——下"         },
    {   6,  6,   170,  100,  135,  1,    "A6",  "L_eyebrow_hor 左眉 前——后"            },
    {   7,  7,   100,  170,  135,  1,    "A7",  "R_eyebrow_hor 右眉 前——后"            },
    {   8,  8,   120,  230,  140,  1,    "A8",  "L_upper_eyelid 左上眼皮 开——合"       },
    {   9,  9,   145,   35,  125,  1,    "A9",  "R_upper_eyelid 右上眼皮 开——合"       },
    {  10, 10,   160,  105,  135,  1,    "A10", "L_vertical_eyeball 左眼球 上——下"     },
    {  11, 11,   110,  165,  135,  1,    "A11", "R_vertical_eyeball 右眼球 上——下"     },
    {  12, 12,   150,  115,  135,  1,    "A12", "L_horizontal_eyeball 左眼球 内——外"   },
    {  13, 13,   120,  155,  135,  1,    "A13", "R_horizontal_eyeball 右眼球 内——外"   },
    {  14, 14,    95,  135,  135,  1,    "A14", "R_cheek 右脸颊 下——上"                },
    {  15, 15,   175,  135,  135,  1,    "A15", "L_cheek 左脸颊 上——下"                },
    {  16, 16,    95,  190,  147,  1,    "A16", "mid_upper_lip_ver 中上嘴唇 上——下"    },
    {  17, 17,   160,  200,  190,  1,    "A17", "mid_upper_lip_hor 中上嘴唇 前——后"    },
    {  18, 18,   185,  155,  165,  1,    "A18", "mid_lower_lip_hor 中下嘴唇 前——后"    },
    {  19, 19,   200,  100,  155,  1,    "A19", "mid_lower_lip_ver 中下嘴唇 上——下"    },
    {  20, 20,   160,   70,  115,  1,    "A20", "upper_lip 上嘴唇 上——下"              },
    {  21, 21,    60,  160,  105,  1,    "A21", "lower_lip 下嘴唇 上——下"              },
    {  22, 22,   160,  100,  135,  1,    "A22", "R_upper_mouth 右嘴角 前——后"          },
    {  23, 23,   180,   90,  135,  1,    "A23", "L_lower_mouth 左嘴角 上——下"          },
    {  24, 24,   105,  165,  130,  1,    "A24", "L_upper_mouth 左嘴角 前——后"          },
    {  25, 25,    90,  180,  135,  1,    "A25", "R_lower_mouth 右嘴角 上——下"          },
    {  26, 26,    85,  150,   95,  1,    "A26", "chin_vertical 下巴 合——开"             },
};

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_CONFIG_H */
