  


#ifndef APP_MENU_H
#define APP_MENU_H

#include "driver/keyboard.h"

#ifdef ENABLE_F_CAL_MENU
    void writeXtalFreqCal(const int32_t value, const bool update_eeprom);
#endif

extern uint8_t gUnlockAllTxConfCnt;

int MENU_GetLimits(uint8_t menu_id, int32_t *pMin, int32_t *pMax);
void MENU_AcceptSetting(void);
void MENU_ShowCurrentSetting(void);
void MENU_StartCssScan(void);
void MENU_CssScanFound(void);
void MENU_StopCssScan(void);
void MENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif

