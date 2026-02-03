 

#ifndef HELPER_BOOT_H
#define HELPER_BOOT_H

#include <stdint.h>
#include "driver/keyboard.h"

enum BOOT_Mode_t
{
    BOOT_MODE_NORMAL = 0,
    BOOT_MODE_F_LOCK,

    #ifdef ENABLE_AIRCOPY
        BOOT_MODE_AIRCOPY
    #endif
};

typedef enum BOOT_Mode_t BOOT_Mode_t;

BOOT_Mode_t BOOT_GetMode(void);
void BOOT_ProcessMode(BOOT_Mode_t Mode);

#endif

