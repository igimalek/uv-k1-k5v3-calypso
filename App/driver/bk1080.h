 

#ifndef DRIVER_BK1080_H
#define DRIVER_BK1080_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/bk1080-regs.h"

extern uint16_t BK1080_BaseFrequency;
extern uint16_t BK1080_FrequencyDeviation;

void BK1080_Init0(void);
void BK1080_Init(uint16_t Frequency, uint8_t band );
uint16_t BK1080_ReadRegister(BK1080_Register_t Register);
void BK1080_WriteRegister(BK1080_Register_t Register, uint16_t Value);
void BK1080_Mute(bool Mute);
uint16_t BK1080_GetFreqLoLimit(uint8_t band);
uint16_t BK1080_GetFreqHiLimit(uint8_t band);
void BK1080_SetFrequency(uint16_t frequency, uint8_t band );
void BK1080_GetFrequencyDeviation(uint16_t Frequency);

#endif

