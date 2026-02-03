 

#include "driver/system.h"
#include "driver/systick.h"

void SYSTEM_DelayMs(uint32_t Delay)
{
    SYSTICK_DelayUs(Delay * 1000);
}

void SYSTEM_ConfigureClocks(void)
{
}
