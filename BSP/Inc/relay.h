/**
 ******************************************************************************
 * @file    relay.h
 * @brief   RS485 Relay module driver — Modbus RTU master over USART3
 * @note    Controls relay coils via Modbus FC 05 (Write Single Coil).
 *          RS485 transceiver uses auto-direction, no DE/RE pin needed.
 *
 *          Coil address mapping:
 *            Channel 0~15  →  Modbus coil 16~31 (relay state)
 *            Coils 0~15 are power-off hold settings, not used here.
 ******************************************************************************
 */

#ifndef __RELAY_H
#define __RELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Modbus coil values --------------------------------------------------------*/
#define RELAY_COIL_ON    0xFF00
#define RELAY_COIL_OFF   0x0000

/* API Functions -------------------------------------------------------------*/

/**
 * @brief  Initialize relay module with Modbus slave address
 * @param  slave_addr: Modbus RTU slave address (1 ~ 247)
 */
void Relay_Init(uint8_t slave_addr);

/**
 * @brief  Turn on relay channel (Modbus FC 05 Write Single Coil = ON)
 * @param  channel: relay coil number (0-based)
 * @return 0 on success, -1 on timeout or CRC error
 */
int Relay_On(uint8_t channel);

/**
 * @brief  Turn off relay channel (Modbus FC 05 Write Single Coil = OFF)
 * @param  channel: relay coil number (0-based)
 * @return 0 on success, -1 on timeout or CRC error
 */
int Relay_Off(uint8_t channel);

/**
 * @brief  Read relay channel status (Modbus FC 01 Read Coils)
 * @param  channel: relay coil number (0-based)
 * @param  status: [out] 1 = ON, 0 = OFF
 * @return 0 on success, -1 on timeout or CRC error
 */
int Relay_Read(uint8_t channel, uint8_t *status);
int Relay_Toggle(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* __RELAY_H */
