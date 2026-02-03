 

#include <stdbool.h>
#include <string.h>
#include "app/scanner.h"
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/scanner.h"

void UI_DisplayScanner(void)
{
    char  String[16] = {0};
    char *pPrintStr = String;
    bool bCentered;
    uint8_t Start;

    UI_DisplayClear();

    if (gScanSingleFrequency || (gScanCssState != SCAN_CSS_STATE_OFF && gScanCssState != SCAN_CSS_STATE_FAILED)) {
        sprintf(String, "FREQ:%u.%05u", gScanFrequency / 100000, gScanFrequency % 100000);
        pPrintStr = String;
    } else {
        pPrintStr = "FREQ:**.*****";
    }

    UI_PrintString(pPrintStr, 2, 0, 1, 8);

    if (gScanCssState < SCAN_CSS_STATE_FOUND || !gScanUseCssResult) {
        pPrintStr = "CTC:******";
    } else if (gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE) {
        sprintf(String, "CTC:%u.%uHz", CTCSS_Options[gScanCssResultCode] / 10, CTCSS_Options[gScanCssResultCode] % 10);
        pPrintStr = String;
    } else {
        sprintf(String, "DCS:D%03oN", DCS_Options[gScanCssResultCode]);
        pPrintStr = String;
    }

    UI_PrintString(pPrintStr, 2, 0, 3, 8);
    memset(String, 0, sizeof(String));
    if (gScannerSaveState == SCAN_SAVE_CHANNEL) {
        pPrintStr = "SAVE?";
        Start     = 0;
        bCentered = 1;
    } else {
        Start     = 2;
        bCentered = 0;

        if (gScannerSaveState == SCAN_SAVE_CHAN_SEL) {
            strcpy(String, "SAVE:");
            UI_GenerateChannelStringEx(String + 5, gShowChPrefix, gScanChannel);
            pPrintStr = String;
        } else if (gScanCssState < SCAN_CSS_STATE_FOUND) {
            strcpy(String, "SCAN");
            memset(String + 4, '.', (gScanProgressIndicator & 7) + 1);
            pPrintStr = String;
        } else if (gScanCssState == SCAN_CSS_STATE_FOUND) {
            pPrintStr = "SCAN CMP.";
        } else {
            pPrintStr = "SCAN FAIL.";
        }
    }

    UI_PrintString(pPrintStr, Start, bCentered ? 127 : 0, 5, 8);

    ST7565_BlitFullScreen();
}
