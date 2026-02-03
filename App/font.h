 

#ifndef FONT_H
#define FONT_H

#include <stdint.h>


extern const uint8_t gFontBig[95 - 1][16 - 2];
extern const uint8_t gFontBigDigits[11][26 - 6];
extern const uint8_t gFont3x5[96][3];
extern const uint8_t gFontSmall[95 - 1][6];
#ifdef ENABLE_SMALL_BOLD
    extern const uint8_t gFontSmallBold[95 - 1][6];
#endif

#endif

