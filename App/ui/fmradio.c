 

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/fm.h"
#include "driver/bk1080.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/fmradio.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void UI_DisplayFM(void)
{
    char String[16] = {0};
    char *pPrintStr = String;
    UI_DisplayClear();

    UI_PrintString("FM", 2, 0, 0, 8);

    sprintf(String, "%d%s-%dM", 
        BK1080_GetFreqLoLimit(gEeprom.FM_Band)/10,
        gEeprom.FM_Band == 0 ? ".5" : "",
        BK1080_GetFreqHiLimit(gEeprom.FM_Band)/10
        );
    
    UI_PrintStringSmallNormal(String, 1, 0, 6);

    if (gAskToSave) {
        pPrintStr = "SAVE?";
    } else if (gAskToDelete) {
        pPrintStr = "DEL?";
    } else if (gFM_ScanState == FM_SCAN_OFF) {
        if (gEeprom.FM_IsMrMode) {
            sprintf(String, "MR(CH%02u)", gEeprom.FM_SelectedChannel + 1);
            pPrintStr = String;
        } else {
            pPrintStr = "VFO";
            for (unsigned int i = 0; i < 20; i++) {
                if (gEeprom.FM_FrequencyPlaying == gFM_Channels[i]) {
                    sprintf(String, "VFO(CH%02u)", i + 1);
                    pPrintStr = String;
                    break;
                }
            }
        }
    } else if (gFM_AutoScan) {
        sprintf(String, "A-SCAN(%u)", gFM_ChannelPosition + 1);
        pPrintStr = String;
    } else {
        pPrintStr = "M-SCAN";
    }

    UI_PrintString(pPrintStr, 0, 127, 3, 10);  

    memset(String, 0, sizeof(String));
    if (gAskToSave || (gEeprom.FM_IsMrMode && gInputBoxIndex > 0)) {
        UI_GenerateChannelString(String, gFM_ChannelPosition);
    } else if (gAskToDelete) {
        sprintf(String, "CH-%02u", gEeprom.FM_SelectedChannel + 1);
    } else {
        if (gInputBoxIndex == 0) {
            sprintf(String, "%3d.%d", gEeprom.FM_FrequencyPlaying / 10, gEeprom.FM_FrequencyPlaying % 10);
        } else {
            const char * ascii = INPUTBOX_GetAscii();
            sprintf(String, "%.3s.%.1s",ascii, ascii + 3);
        }

        UI_DisplayFrequency(String, 36, 1, gInputBoxIndex == 0);   
        ST7565_BlitFullScreen();
        return;
    }

    UI_PrintString(String, 0, 127, 1, 10);

    ST7565_BlitFullScreen();
}

#endif
