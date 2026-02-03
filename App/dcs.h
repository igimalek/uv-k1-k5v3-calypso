 

#ifndef DCS_H
#define DCS_H

#include <stdint.h>

enum DCS_CodeType_t
{
    CODE_TYPE_OFF = 0,
    CODE_TYPE_CONTINUOUS_TONE,
    CODE_TYPE_DIGITAL,
    CODE_TYPE_REVERSE_DIGITAL
};

typedef enum DCS_CodeType_t DCS_CodeType_t;

enum {
    CDCSS_POSITIVE_CODE = 1U,
    CDCSS_NEGATIVE_CODE = 2U,
};

extern const uint16_t CTCSS_Options[50];
extern const uint16_t DCS_Options[104];

uint32_t DCS_GetGolayCodeWord(DCS_CodeType_t CodeType, uint8_t Option);
uint8_t DCS_GetCdcssCode(uint32_t Code);
uint8_t DCS_GetCtcssCode(int Code);

#endif

