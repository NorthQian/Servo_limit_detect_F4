#ifndef _APP_RELAY_H_
#define _APP_RELAY_H_

#include <stdint.h>

void RELAY_TASK_ENTRY(void *argument);
uint8_t Relay_GetState(void);

#endif
