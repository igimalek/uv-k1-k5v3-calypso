  

#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>

#include "functions.h"
#include "frequencies.h"
#include "radio.h"

void     APP_EndTransmission(void);
void     APP_StartListening(FUNCTION_Type_t function);
uint32_t APP_SetFreqByStepAndLimits(VFO_Info_t *pInfo, int8_t direction, uint32_t lower, uint32_t upper);
uint32_t APP_SetFrequencyByStep(VFO_Info_t *pInfo, int8_t direction);
void     APP_Update(void);
void     APP_TimeSlice10ms(void);
void     APP_TimeSlice500ms(void);

#endif

