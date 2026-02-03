 

#ifndef DRIVER_EEPROM_H
#define DRIVER_EEPROM_H

#include <stdint.h>

void EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size);
void EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer);

#endif

