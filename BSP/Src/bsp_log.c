#include "bsp_log.h"
#include "stdlib.h"

#if LOG_USE_UART
#include "usart.h"

#define LOG_BUF_SIZE  256

void BSPLogInit()
{
    /* UART already initialized by MX_USART1_UART_Init(), nothing to do */
}

int PrintLog(const char *fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, LOG_BUF_SIZE, fmt, args);
    va_end(args);

    if (n > 0) {
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, n, HAL_MAX_DELAY);
    }
    return n;
}

#else /* LOG_USE_UART == 0: SEGGER RTT */

void BSPLogInit()
{
    SEGGER_RTT_Init();
}

int PrintLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = SEGGER_RTT_vprintf(BUFFER_INDEX, fmt, &args);
    va_end(args);
    return n;
}

#endif /* LOG_USE_UART */

void Float2Str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = abs(head);
    point = abs(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}

