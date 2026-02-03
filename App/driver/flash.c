 

#include "driver/flash.h"
#include "sram-overlay.h"

void FLASH_Init(FLASH_READ_MODE ReadMode)
{
    overlay_FLASH_Init(ReadMode);
}

void FLASH_ConfigureTrimValues(void)
{
    overlay_FLASH_ConfigureTrimValues();
}

uint32_t FLASH_ReadNvrWord(uint32_t Address)
{
    return overlay_FLASH_ReadNvrWord(Address);
}
