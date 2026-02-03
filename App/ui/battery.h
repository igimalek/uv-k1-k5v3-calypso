 

#ifndef UI_BATTERY_H
#define UI_BATTERY_H

#include <stdint.h>
void UI_DrawBattery(uint8_t* bitmap, uint8_t level, uint8_t blink);
void UI_DisplayBattery(uint8_t Level, uint8_t blink);

#endif

