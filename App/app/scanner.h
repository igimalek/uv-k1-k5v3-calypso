  

#ifndef APP_SCANNER_H
#define APP_SCANNER_H

#include "dcs.h"
#include "driver/keyboard.h"

typedef enum 
{
    SCAN_CSS_STATE_OFF,
    SCAN_CSS_STATE_SCANNING,
    SCAN_CSS_STATE_FOUND,
    SCAN_CSS_STATE_FAILED
} SCAN_CssState_t;

typedef enum 
{
    SCAN_SAVE_NO_PROMPT,   
    SCAN_SAVE_CHAN_SEL,    
    SCAN_SAVE_CHANNEL,     
} SCAN_SaveState_t;


extern DCS_CodeType_t    gScanCssResultType;
extern uint8_t           gScanCssResultCode;
extern bool              gScanSingleFrequency;
extern SCAN_SaveState_t  gScannerSaveState;
extern uint8_t           gScanChannel;
extern uint32_t          gScanFrequency;
extern SCAN_CssState_t   gScanCssState;
extern uint8_t           gScanProgressIndicator;
extern bool              gScanUseCssResult;

void SCANNER_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void SCANNER_Start(bool singleFreq);
void SCANNER_Stop(void);
void SCANNER_TimeSlice10ms(void);
void SCANNER_TimeSlice500ms(void);
bool SCANNER_IsScanning(void);

#endif

