 

#ifndef DRIVER_VOICE_H
#define DRIVER_VOICE_H

#include <stdint.h>

#define VOICE_BUF_CAP 4
#define VOICE_BUF_LEN 160

extern uint16_t gVoiceBuf[VOICE_BUF_CAP][VOICE_BUF_LEN];
extern uint8_t gVoiceBufReadIndex;
extern uint8_t gVoiceBufWriteIndex;
extern uint8_t gVoiceBufLen;

static inline void VOICE_BUF_ForwardReadIndex()
{
    gVoiceBufReadIndex = (1 + gVoiceBufReadIndex) % VOICE_BUF_CAP;
}

static inline void VOICE_BUF_ForwardWriteIndex()
{
    gVoiceBufWriteIndex = (1 + gVoiceBufWriteIndex) % VOICE_BUF_CAP;
}

void VOICE_Init();
void VOICE_Start();
void VOICE_Stop();

#endif  
