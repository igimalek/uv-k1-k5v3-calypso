#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "app/scanner.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
 
static void convertTime(uint8_t *line, uint8_t type) 
{
    uint16_t t = (type == 0) ? (gTxTimerCountdown_500ms / 2) : (3600 - gRxTimerCountdown_500ms / 2);
    uint8_t m = t / 60;
    uint8_t s = t % 60;
    char str[6];
    sprintf(str, "%02u:%02u", m, s);
    UI_PrintStringSmallBufferNormal(str, line);
    gUpdateStatus = true;
}
#endif

void UI_DisplayStatus()
{
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    char str[12];


    const uint8_t POS_TMR  = 0;    
    const uint8_t POS_MOD  = 38;   
    const uint8_t POS_VOX  = 56;   
    const uint8_t POS_PTT  = 66;   
    
    const uint8_t POS_B    = 75;   
    const uint8_t POS_LOCK = 84;   
    const uint8_t POS_F    = 84;   


#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
    if (gSetting_set_tmr) {
        if (gCurrentFunction == FUNCTION_TRANSMIT) {
            convertTime(gStatusLine + POS_TMR, 0);
        } else if (FUNCTION_IsRx()) {
            convertTime(gStatusLine + POS_TMR, 1);
        }
    }
#endif

     
    if (!SCANNER_IsScanning()) {
        uint8_t dw = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2;
        char* modeTag = (dw == 1 || dw == 3) ? (gDualWatchActive ? "DWR" : "HLD") : (dw == 2 ? "XB" : "MO");
        UI_PrintStringSmallBufferNormal(modeTag, gStatusLine + POS_MOD);
    }

     
#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH) {
        UI_PrintStringSmallBufferNormal("VOX", gStatusLine + POS_VOX);
    }
#endif

     
    if (gSetting_set_ptt_session) {
        gStatusLine[POS_PTT + 0] |= 0x3E;
        gStatusLine[POS_PTT + 1] |= 0x63;
        gStatusLine[POS_PTT + 2] |= 0x55;
        gStatusLine[POS_PTT + 3] |= 0x49;
        gStatusLine[POS_PTT + 4] |= 0x41;
        gStatusLine[POS_PTT + 5] |= 0x3E;
    } else {
        gStatusLine[POS_PTT + 0] |= 0x3E;
        gStatusLine[POS_PTT + 1] |= 0x7F;
        gStatusLine[POS_PTT + 2] |= 0x5D;
        gStatusLine[POS_PTT + 3] |= 0x49;
        gStatusLine[POS_PTT + 4] |= 0x41;
        gStatusLine[POS_PTT + 5] |= 0x3E;
    }


    if (gBackLight) {
        if (gEeprom.BACKLIGHT_TIME == 0) {
             
            gStatusLine[POS_B + 0] |= 0x0C;
            gStatusLine[POS_B + 1] |= 0x12;
            gStatusLine[POS_B + 2] |= 0x65;
            gStatusLine[POS_B + 3] |= 0x79;
            gStatusLine[POS_B + 4] |= 0x65;
            gStatusLine[POS_B + 5] |= 0x12;
            gStatusLine[POS_B + 6] |= 0x0C;
        } else {
             
            gStatusLine[POS_B + 0] |= 0x0C;
            gStatusLine[POS_B + 1] |= 0x12;
            gStatusLine[POS_B + 2] |= 0x65;
            gStatusLine[POS_B + 3] |= 0x79;
            gStatusLine[POS_B + 4] |= 0x65;
            gStatusLine[POS_B + 5] |= 0x12;
            gStatusLine[POS_B + 6] |= 0x0C;
        }
    }

     
    if (gWasFKeyPressed) {
        gStatusLine[POS_F + 0] |= 0x7F;
        gStatusLine[POS_F + 1] |= 0x41;
        gStatusLine[POS_F + 2] |= 0x75;
        gStatusLine[POS_F + 3] |= 0x75;
        gStatusLine[POS_F + 4] |= 0x75;
        gStatusLine[POS_F + 5] |= 0x7D;
        gStatusLine[POS_F + 6] |= 0x7F;
    } else if (gEeprom.KEY_LOCK) {
        gStatusLine[POS_LOCK + 0] |= 0x7C;
        gStatusLine[POS_LOCK + 1] |= 0x7A;
        gStatusLine[POS_LOCK + 2] |= 0x79;
        gStatusLine[POS_LOCK + 3] |= 0x49;
        gStatusLine[POS_LOCK + 4] |= 0x79;
        gStatusLine[POS_LOCK + 5] |= 0x7A;
        gStatusLine[POS_LOCK + 6] |= 0x7C;
    }

     
    if (gSetting_battery_text == 1) {
        sprintf(str, "%u.%02u", gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100);
    } else {
        sprintf(str, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
    }
    
    uint8_t battPos = 127 - (strlen(str) * 7);
    UI_PrintStringSmallBufferBold(str, gStatusLine + battPos);

    ST7565_BlitStatusLine();
}