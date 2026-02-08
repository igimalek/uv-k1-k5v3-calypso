

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "py32f0xx.h"

static void inline SCHEDULER_Enable()
{
    NVIC_EnableIRQ(SysTick_IRQn);
}

static void inline SCHEDULER_Disable()
{
    NVIC_DisableIRQ(SysTick_IRQn);
}

#endif
