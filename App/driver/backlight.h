 

#ifndef DRIVER_BACKLIGHT_H
#define DRIVER_BACKLIGHT_H

#include <stdint.h>
#include <stdbool.h>

extern uint16_t gBacklightCountdown_500ms;
extern uint8_t gBacklightBrightness;

#ifdef ENABLE_FEAT_F4HWN
    extern const uint8_t value[11];
#endif

#ifdef ENABLE_FEAT_F4HWN_SLEEP
    extern uint16_t gSleepModeCountdown_500ms;
#endif

#ifdef ENABLE_BLMIN_TMP_OFF
typedef enum {
    BLMIN_STAT_ON,
    BLMIN_STAT_OFF,
    BLMIN_STAT_UNKNOWN
} BLMIN_STAT_t;
#endif

void BACKLIGHT_InitHardware();
void BACKLIGHT_TurnOn();
void BACKLIGHT_TurnOff();
bool BACKLIGHT_IsOn();
void BACKLIGHT_SetBrightness(uint8_t brigtness);
uint8_t BACKLIGHT_GetBrightness(void);

#endif
