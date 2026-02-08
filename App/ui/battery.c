

#include <stddef.h>
#include <string.h>

#include "bitmaps.h"
#include "driver/st7565.h"
#include "functions.h"
#include "ui/battery.h"
#include "../misc.h"

void UI_DrawBattery(uint8_t* bitmap, uint8_t level, uint8_t blink)
{
    if (level < 2 && blink == 1) {
        memset(bitmap, 0, sizeof(BITMAP_BatteryLevel1));
        return;
    }

    memcpy(bitmap, BITMAP_BatteryLevel1, sizeof(BITMAP_BatteryLevel1));

    if (level <= 2) {
        return;
    }

    const uint8_t bars = MIN(4, level - 2);

    for (int i = 0; i < bars; i++) {
#ifndef ENABLE_REVERSE_BAT_SYMBOL
        memcpy(bitmap + sizeof(BITMAP_BatteryLevel1) - 4 - (i * 3), BITMAP_BatteryLevel, 2);
#else
        memcpy(bitmap + 3 + (i * 3) + 0, BITMAP_BatteryLevel, 2);
#endif
    }
}

void UI_DisplayBattery(uint8_t level, uint8_t blink)
{
    uint8_t bitmap[sizeof(BITMAP_BatteryLevel1)];
    UI_DrawBattery(bitmap, level, blink);
    ST7565_DrawLine(LCD_WIDTH - sizeof(bitmap), 0, bitmap, sizeof(bitmap));
}
