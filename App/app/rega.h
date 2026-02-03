  

#ifndef REGA_H
#define REGA_H

#include <stdint.h>

#define ZVEI_NUM_TONES 5
#define ZVEI_TONE_LENGTH_MS 70   
#define ZVEI_PAUSE_LENGTH_MS 10   
#define ZVEI_PRE_LENGTH_MS 300
#define ZVEI_POST_LENGTH_MS 100
#define REGA_CTCSS_FREQ_INDEX 18   
#define REGA_FREQUENCY 16130000   

void ACTION_RegaAlarm(void);
void ACTION_RegaTest(void);
void UI_DisplayREGA(void);
void REGA_TransmitZvei(const uint16_t[], const char[]);

#endif
