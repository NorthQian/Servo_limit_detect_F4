#include "current.h"
#include "ina229.h"
#include "cmsis_os2.h" 



/**
 * @brief  Initialize INA229 current sensor
 * @note   Must be called after SPI1 and GPIO are initialized
 */
void Current_Init(void)
{
    Set_SHUNT_CAL(1250);        /* SHUNT_CAL for 5mOhm shunt, 10A max */
    INA229_CheckPresent();      /* Verify device ID = 0x2291 */
}

/**
 * @brief  Read latest current value from INA229
 * @note   Result stored in Ina229.Current (Amps)
 */
void Current_Update(void)
{
    ReadCurrent();
}

/**
 * @brief  Get last read current value
 * @return Current in Amps
 */
float Current_GetValue(void)
{
    return Ina229.Current;
}

void Get_Current(void *argument)
{
  /* USER CODE BEGIN Get_Current */

  /* 初始化INA229 */
  Current_Init();

  /* 循环读取电流 */
  for (;;)
  {
    Current_Update();
    osDelay(10);
  }
  /* USER CODE END Get_Current */
}