/**
 ******************************************************************************
 * @file    ina229.c
 * @brief   TI INA229 precision power/energy monitor driver (HAL SPI version)
 * @note    HAL SPI1: CPOL=0, CPHA=0, MSB first, 8-bit data
 *          CS  = PC4 (GPIO)
 *          SCK = PA5 (SPI1_SCK)
 *          MISO= PA6 (SPI1_MISO)
 *          MOSI= PA7 (SPI1_MOSI)
 *
 *          Calibration formula:
 *          SHUNT_CAL = 13107.2 * 10^6 * CURRENT_LSB * RSHUNT
 *          CURRENT_LSB = MaxExpectedCurrent / 524288
 *
 *          Conversion factors:
 *          VBUS:   195.3125 uV/LSB
 *          VSHUNT: 312.5 nV/LSB (ADCRANGE=0), 78.125 nV/LSB (ADCRANGE=1)
 *          TEMP:   7.8125 mC/LSB
 *          CURRENT = Reg_CURRENT * CURRENT_LSB
 *          POWER   = 3.2 * CURRENT_LSB * Reg_POWER
 *          ENERGY  = 16 * 3.2 * CURRENT_LSB * Reg_ENERGY (Joules)
 *          CHARGE  = CURRENT_LSB * Reg_CHARGE (Coulombs)
 ******************************************************************************
 */

#include "ina229.h"
#include "spi.h"
#include "bsp_LCD_ILI9341.h"

/* Calibration Parameters ----------------------------------------------------*/
#define MaxExpectedCurrent  10          /* Maximum expected current: 10 Amps */
float CURRENT_LSB = (float)MaxExpectedCurrent/524288;

/* CS Pin Macros -------------------------------------------------------------*/
#define CS_1    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)
#define CS_0    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)

/* Global Data ---------------------------------------------------------------*/
INA229 Ina229;

/*============================================================================
 * HAL SPI Helper Functions
 *============================================================================*/

/**
 * @brief  Send command byte + read N bytes from INA229 over HAL SPI
 * @param  addr: register address (6-bit)
 * @param  rxBuf: receive buffer
 * @param  len: number of bytes to read (2/3/5)
 */
static void INA229_ReadReg(uint8_t addr, uint8_t *rxBuf, uint8_t len)
{
    uint8_t cmd   = (uint8_t)((addr << 2) | 0x01);          /* Read command */
    uint8_t tx[6] = {cmd, 0x00, 0x00, 0x00, 0x00, 0x00};   /* cmd + 5 dummy bytes */
    uint8_t rx[6];                                           /* rx[0]=garbage, rx[1..]=data */

    CS_0;
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, len + 1, HAL_MAX_DELAY);
    CS_1;

    for (uint8_t i = 0; i < len; i++)
        rxBuf[i] = rx[i + 1];   /* skip first garbage byte */
}

/**
 * @brief  Send command byte + write N bytes to INA229 over HAL SPI
 * @param  addr: register address (6-bit)
 * @param  txBuf: data buffer to send
 * @param  len: number of bytes to write (2)
 */
static void INA229_WriteReg(uint8_t addr, uint8_t *txBuf, uint8_t len)
{
    uint8_t tx[7] = {(uint8_t)((addr << 2) | 0x00)};  /* [0]=cmd */

    for (uint8_t i = 0; i < len; i++)
        tx[i + 1] = txBuf[i];

    CS_0;
    HAL_SPI_Transmit(&hspi1, tx, len + 1, HAL_MAX_DELAY);
    CS_1;
}

/*============================================================================
 * Register Read/Write Operations
 *============================================================================*/

/**
 * @brief  Generic 16-bit register read from INA229
 * @param  addr: register address (6-bit)
 * @return 16-bit register value
 */
unsigned int ReadINA229(unsigned char addr)
{
    uint8_t rx[2];
    INA229_ReadReg(addr, rx, 2);
    return ((unsigned int)rx[0] << 8) | rx[1];
}

/**
 * @brief  Read any 16-bit register (alias for ReadINA229)
 */
unsigned int ReadU16Reg(unsigned char RegAddr)
{
    return ReadINA229(RegAddr);
}

/*============================================================================
 * High-Level Measurement Functions
 *============================================================================*/

/**
 * @brief  Read bus voltage from INA229
 * @note   Result stored in Ina229.VbusVal (Volts)
 */
void ReadVbus(void)
{
    uint8_t rx[3];

    INA229_ReadReg(VBUS, rx, 3);

    Ina229.Reg_VBUS = ((unsigned long)rx[0] << 16)
                    | ((unsigned long)rx[1] << 8)
                    |  (unsigned long)rx[2];
    Ina229.Reg_VBUS >>= 4;
    Ina229.VbusVal = (float)Ina229.Reg_VBUS * 195.3125f / 1000000.0f;
}

/**
 * @brief  Read die temperature from INA229
 * @note   Result stored in Ina229.Temp (Celsius)
 */
void ReadTemp(void)
{
    Ina229.Reg_DIETEMP = ReadINA229(DIETEMP);
    Ina229.Temp = (float)Ina229.Reg_DIETEMP * 7.8125f / 1000.0f;
}

/**
 * @brief  Read shunt voltage from INA229
 * @note   Result stored in Ina229.Vshunt (microvolts)
 */
void ReadVshunt(void)
{
    uint8_t rx[3];

    INA229_ReadReg(VSHUNT, rx, 3);

    Ina229.Reg_VSHUNT = ((unsigned long)rx[0] << 16)
                      | ((unsigned long)rx[1] << 8)
                      |  (unsigned long)rx[2];
    Ina229.Reg_VSHUNT >>= 4;

    /* 312.5 nV/LSB at ADCRANGE=0 */
    Ina229.Vshunt = (float)(Ina229.Reg_VSHUNT) * 312.5f / 1000.0f;
}

/**
 * @brief  Write SHUNT_CAL register
 * @param  val: 16-bit calibration value
 */
void Set_SHUNT_CAL(unsigned int val)
{
    uint8_t tx[2];

    tx[0] = (uint8_t)(val >> 8);
    tx[1] = (uint8_t)(val);

    INA229_WriteReg(SHUNT_CAL, tx, 2);
}

/**
 * @brief  Read current from INA229
 * @note   Result stored in Ina229.Current (Amps)
 */
void ReadCurrent(void)
{
    uint8_t rx[3];
    float current;

    INA229_ReadReg(CURRENT, rx, 3);

    Ina229.Reg_CURRENT = ((unsigned long)rx[0] << 16)
                       | ((unsigned long)rx[1] << 8)
                       |  (unsigned long)rx[2];
    Ina229.Reg_CURRENT >>= 4;
    current = (float)Ina229.Reg_CURRENT * CURRENT_LSB;

    /* 超过10A视为异常读数 */
    if (current > 10.0f)
    {
        return;
    }else
    {
        Ina229.Current=current;
    }
}

/**
 * @brief  Read power from INA229
 * @note   Result stored in Ina229.Power (Watts)
 */
void ReadPower(void)
{
    uint8_t rx[3];

    INA229_ReadReg(INA229_POWER, rx, 3);

    Ina229.Reg_POWER = ((unsigned long)rx[0] << 16)
                     | ((unsigned long)rx[1] << 8)
                     |  (unsigned long)rx[2];
    Ina229.Power = 3.2f * CURRENT_LSB * (float)Ina229.Reg_POWER;
}

/**
 * @brief  Read energy from INA229
 * @note   Result stored in Ina229.Enerey (Joules)
 */
void ReadEnerjy(void)
{
    uint8_t rx[5];

    INA229_ReadReg(ENERGY, rx, 5);

    Ina229.Reg_ENERGY = ((uint64_t)rx[0] << 32)
                      | ((uint64_t)rx[1] << 24)
                      | ((uint64_t)rx[2] << 16)
                      | ((uint64_t)rx[3] << 8)
                      |  (uint64_t)rx[4];
    Ina229.Enerey = 16.0f * 3.2f * CURRENT_LSB * (float)Ina229.Reg_ENERGY;
}

/**
 * @brief  Read charge from INA229
 * @note   Result stored in Ina229.Charge (Coulombs)
 */
void ReadCharge(void)
{
    uint8_t rx[5];

    INA229_ReadReg(CHARGE, rx, 5);

    Ina229.Reg_CHARGE = ((uint64_t)rx[0] << 32)
                      | ((uint64_t)rx[1] << 24)
                      | ((uint64_t)rx[2] << 16)
                      | ((uint64_t)rx[3] << 8)
                      |  (uint64_t)rx[4];
    Ina229.Charge = CURRENT_LSB * (float)Ina229.Reg_CHARGE;
}

/**
 * @brief  Check if INA229 is present on SPI bus
 * @note   Reads DEVICE_ID (expected 0x2291). Blocks forever if not found,
 *         showing "INA229 NOT FOUND" on OLED and blinking PC13 LED.
 */
void INA229_CheckPresent(void)
{
    Ina229.DeviceID = ReadU16Reg(DEVICE_ID);
    if (Ina229.DeviceID != 0x2291) {
        LCD_Fill(0, 0, LCD_GetWidth() - 1, LCD_GetHeight() - 1, BLACK);
        LCD_String(10, 60, "INA229 NOT FOUND", 16, RED, BLACK);
        LCD_String(10, 90, "Check SPI connect", 12, YELLOW, BLACK);
        while (1) {
            LED_TOGGLE();
            HAL_Delay(100);
        }
    }
}

/**
 * @brief  Read all INA229 measurement registers
 */
void INA229_Read(void)
{
    // ReadVbus();
    // ReadVshunt();
    ReadCurrent();
    // ReadPower();

    // Ina229.Reg_SHUNT_CAL = ReadU16Reg(SHUNT_CAL);
    // Ina229.ManID = ReadU16Reg(MANUFACTURER_ID);
    // Ina229.DeviceID = ReadU16Reg(DEVICE_ID);
}

/**
 * @brief  Display V/I/P readings on OLED
 */
void INA229_Display(void)
{
//    char buf[24];

//    sprintf(buf, "V: %.3f V ", Ina229.VbusVal);
//    OLED_ShowString(0, 2, (uint8_t *)buf, 12);
//    sprintf(buf, "I: %.3f A ", Ina229.Current);
//    OLED_ShowString(0, 4, (uint8_t *)buf, 12);
//    sprintf(buf, "P: %.3f W ", Ina229.Power);
//    OLED_ShowString(0, 6, (uint8_t *)buf, 12);
}
