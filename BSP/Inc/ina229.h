/**
 ******************************************************************************
 * @file    ina229.h
 * @brief   TI INA229 precision power/energy monitor driver
 * @note    Hardware connections:
 *          INA229_CS   --> PB1
 *          INA229_SDI  --> PA7 (MOSI)
 *          INA229_SCK  --> PA5
 *          INA229_SDO  --> PA6 (MISO)
 *          Communication: Bit-banged SPI (CPOL=0, CPHA=0)
 ******************************************************************************
 */

#ifndef __INA229_H
#define __INA229_H

#include "main.h"

/* INA229 Register Map -------------------------------------------------------*/
#define CONFIG          0x00    /* Configuration */
#define ADC_CONFIG      0x01    /* ADC Configuration */
#define SHUNT_CAL       0x02    /* Shunt Calibration */
#define SHUNT_TEMPCO    0x03    /* Shunt Temperature Coefficient */
#define VSHUNT          0x04    /* Shunt Voltage Result (24-bit) */
#define VBUS            0x05    /* Bus Voltage Result (24-bit) */
#define DIETEMP         0x06    /* Die Temperature (16-bit) */
#define CURRENT         0x07    /* Current Result (24-bit) */
#define INA229_POWER    0x08    /* Power Result (24-bit) */
#define ENERGY          0x09    /* Energy Result (40-bit) */
#define CHARGE          0x0A    /* Charge Result (40-bit) */
#define DIAG_ALRT       0x0B    /* Diagnostics and Alerts */
#define SOVL            0x0C    /* Shunt Overvoltage Limit */
#define SUVL            0x0D    /* Shunt Undervoltage Limit */
#define BOVL            0x0E    /* Bus Overvoltage Limit */
#define BUVL            0x0F    /* Bus Undervoltage Limit */
#define TEMP_LIMIT      0x10    /* Temperature Over-Limit */
#define PWR_LIMIT       0x11    /* Power Over-Limit */
#define MANUFACTURER_ID 0x3E    /* Manufacturer ID (default: 0x5449) */
#define DEVICE_ID       0x3F    /* Device ID (default: 0x2291) */

/* INA229 Data Structure -----------------------------------------------------*/
typedef struct
{
    unsigned int  Reg_CONFIG;
    unsigned int  Reg_ADC_CONFIG;
    unsigned int  Reg_SHUNT_CAL;
    unsigned int  Reg_TEPMCO;
    unsigned long Reg_VSHUNT;
    unsigned long Reg_VBUS;
    unsigned int  Reg_DIETEMP;
    unsigned long Reg_CURRENT;
    unsigned long Reg_POWER;
    uint64_t      Reg_ENERGY;
    uint64_t      Reg_CHARGE;
    unsigned int  Reg_DIAG_ALRT;
    unsigned int  Reg_SOVL;
    unsigned int  Reg_SUVL;
    unsigned int  Reg_BOVL;
    unsigned int  Reg_BUVL;
    unsigned int  Reg_TEMP_LIMIT;
    unsigned int  Reg_PWR_LIMIT;
    unsigned int  Reg_MANUFACTURER_ID;
    unsigned int  Reg_DEVICE_ID;

    float Temp;
    float VbusVal;
    float Vshunt;
    float Current;
    float Enerey;
    float Charge;
    unsigned int ManID;
    unsigned int DeviceID;
    float Power;
} INA229;

extern INA229 Ina229;

/* API Functions -------------------------------------------------------------*/
unsigned int ReadINA229(unsigned char addr);
unsigned int ReadU16Reg(unsigned char RegAddr);
void ReadVbus(void);
void ReadTemp(void);
void ReadVshunt(void);
void Set_SHUNT_CAL(unsigned int val);
void ReadCurrent(void);
void ReadPower(void);
void ReadEnerjy(void);
void ReadCharge(void);
void INA229_CheckPresent(void);
void INA229_Read(void);
void INA229_Display(void);

#endif /* __INA229_H */
