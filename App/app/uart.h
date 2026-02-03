  

#ifndef APP_UART_H
#define APP_UART_H

#include <stdbool.h>

enum
{
#if defined(ENABLE_UART)
    UART_PORT_UART,
#endif
#if defined(ENABLE_USB)
    UART_PORT_VCP,
#endif
};

bool UART_IsCommandAvailable(uint32_t Port);
void UART_HandleCommand(uint32_t Port);

#endif

