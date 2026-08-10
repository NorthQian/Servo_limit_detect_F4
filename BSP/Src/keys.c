#include "keys.h"
#include "cmsis_os2.h"

KeyInstance KEY1_Instance = {GPIOA, GPIO_PIN_0, 0, 1, GPIO_PIN_SET};
KeyInstance KEY2_Instance = {GPIOA, GPIO_PIN_1, 0, 2, GPIO_PIN_RESET};
KeyInstance KEY3_Instance = {GPIOA, GPIO_PIN_4, 0, 3, GPIO_PIN_RESET};

extern osSemaphoreId_t Key1SemaHandle;
extern osSemaphoreId_t Key2SemaHandle;
extern osSemaphoreId_t Key3SemaHandle;

/**
 * @brief Key scan funtion
 *
 * @param Key_Instance
 */
void Key_Scan(KeyInstance *p)
{
    switch (p->key_stg)
    {
    case 0:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) == p->key_push_level)
        {
            p->key_stg = 1;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 1:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) == p->key_push_level)
        {
            p->key_stg = 2;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 2:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) != p->key_push_level)
        {
            p->key_stg = 0;

            switch (p->key_flag)
            {
            case 1:
                osSemaphoreRelease(Key1SemaHandle);
                break;
            case 2:
                osSemaphoreRelease(Key2SemaHandle);
                break;
            case 3:
                osSemaphoreRelease(Key3SemaHandle);
                break;
            }
        }
        else
        {
            p->key_stg = 2;
        }

        break;
    }
}

void App_Key_Scan(void *argument)
{
    /* USER CODE BEGIN App_Key_Scan */
    /* Infinite loop */
    for (;;)
    {

        Key_Scan(&KEY1_Instance);
        Key_Scan(&KEY2_Instance);
        Key_Scan(&KEY3_Instance);
        osDelay(10);
    }
    /* USER CODE END App_Key_Scan */
}
