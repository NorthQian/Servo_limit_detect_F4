#include "app_relay.h"
#include "relay.h"
#include "cmsis_os2.h"

extern osSemaphoreId_t Key1SemaHandle;
extern osSemaphoreId_t Key2SemaHandle;
extern osSemaphoreId_t Key3SemaHandle;

static uint8_t relay_state = 0;  /* 默认上电接通: 继电器断开,NC闭合,负载有电 */

uint8_t Relay_GetState(void)
{
    return relay_state;
}

void App_relay(void *argument)
{
  /* USER CODE BEGIN App_relay */

  Relay_Init(1);
  osDelay(100);

  /* 上电同步硬件继电器状态：确保线圈断开(NC闭合)，与relay_state=0一致 */
  Relay_Off(0);

  /* Infinite loop */
  for (;;)
  {
    osSemaphoreAcquire(Key1SemaHandle, osWaitForever);

    /* 翻转第1路继电器 */
    if (relay_state) {
      Relay_Off(0);
      relay_state = 0;
    } else {
      Relay_On(0);
      relay_state = 1;
    }
  }
  /* USER CODE END App_relay */
}