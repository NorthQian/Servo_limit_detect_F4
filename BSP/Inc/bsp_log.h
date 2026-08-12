#ifndef _BSP_LOG_H
#define _BSP_LOG_H

// ARM CLANG toolchain's stdarg.h does not define va_list, but __builtin_va_list,
// and the va_arg macro is defined in __stdarg_va_arg.h

#include "__stdarg_va_list.h"
#include "__stdarg_va_arg.h"
#include <stdio.h>

/* ==========================================================================
 *  Output mode switch
 *    0 = SEGGER RTT (default, requires J-Link debug probe)
 *    1 = UART1     (PA9 TX, 115200 8N1, via HAL_UART_Transmit)
 * ========================================================================== */
#define LOG_USE_UART    1

/* --- RTT-specific includes and macros -------------------------------- */
#if LOG_USE_UART == 0
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

#define BUFFER_INDEX    0

extern int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList);

#define LOG_CLEAR()     SEGGER_RTT_WriteString(0, "  " RTT_CTRL_CLEAR)

#define LOG_PROTO(type, color, format, ...)                       \
        SEGGER_RTT_printf(BUFFER_INDEX, "  %s%s" format "\r\n%s", \
                          color,                                  \
                          type,                                   \
                          ##__VA_ARGS__,                          \
                          RTT_CTRL_RESET)

#define LOG(format, ...)       LOG_PROTO("", "", format, ##__VA_ARGS__)
#define LOGINFO(format, ...)   LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN,  format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
#define LOGERROR(format, ...)  LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED,    format, ##__VA_ARGS__)

/* --- UART-specific macros (plain text, no ANSI codes) --------------- */
#else

#define LOG_CLEAR()     /* no-op on UART */

#define LOG_PROTO(type, junk, format, ...)  \
        PrintLog("  %s" format "\r\n", type, ##__VA_ARGS__)

#define LOG(format, ...)       LOG_PROTO("",    0, format, ##__VA_ARGS__)
#define LOGINFO(format, ...)   LOG_PROTO("I:",  0, format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", 0, format, ##__VA_ARGS__)
#define LOGERROR(format, ...)  LOG_PROTO("E:",  0, format, ##__VA_ARGS__)

#endif /* LOG_USE_UART */

/**
 * @brief 日志系统初始化
 */
void BSPLogInit();

/**
 * @brief 格式化打印,根据 LOG_USE_UART 开关自动选择 RTT 或 UART1 输出
 * @note  不支持浮点格式化,浮点请先调用 Float2Str() 转换
 * @param fmt 格式字符串
 * @param ... 参数列表
 * @return int 打印的字符数
 */
int PrintLog(const char *fmt, ...);

/**
 * @brief 利用sprintf(),将float转换为字符串进行打印
 */
void Float2Str(char *str, float va);

#endif