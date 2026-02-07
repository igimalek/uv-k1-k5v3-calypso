

#include <string.h>

#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"
#include "bitmaps.h"

#ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
    #include "screenshot.h"
#endif

void UI_DisplayReleaseKeys(void)
{
    memset(gStatusLine,  0, sizeof(gStatusLine));
#if defined(ENABLE_FEAT_F4HWN_CTR) || defined(ENABLE_FEAT_F4HWN_INV)
        ST7565_ContrastAndInv();
#endif
    UI_DisplayClear();

    UI_PrintString("RELEASE", 0, 127, 1, 10);
    UI_PrintString("ALL KEYS", 0, 127, 3, 10);

    ST7565_BlitStatusLine();  
    ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
    char WelcomeString0[16];
    char WelcomeString1[16];
    char WelcomeString2[16];
    char WelcomeString3[20];

#if defined(ENABLE_FEAT_F4HWN_CTR) || defined(ENABLE_FEAT_F4HWN_INV)
        ST7565_ContrastAndInv();
#endif
    UI_DisplayClear();

#ifdef ENABLE_FEAT_F4HWN
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
    
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_SOUND) {
        ST7565_FillScreen(0x00);
#else
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_FULL_SCREEN) {
        ST7565_FillScreen(0xFF);
#endif
    } else {
        memset(WelcomeString0, 0, sizeof(WelcomeString0));
        memset(WelcomeString1, 0, sizeof(WelcomeString1));

        
        PY25Q16_ReadBuffer(0x007020, WelcomeString0, 16);
        
        PY25Q16_ReadBuffer(0x007030, WelcomeString1, 16);

        sprintf(WelcomeString2, "%u.%02uV %u%%",
                gBatteryVoltageAverage / 100,
                gBatteryVoltageAverage % 100,
                BATTERY_VoltsToPercent(gBatteryVoltageAverage));

        if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_VOLTAGE)
        {
            strcpy(WelcomeString0, "VOLTAGE");
            strcpy(WelcomeString1, WelcomeString2);
        }
        else if(gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_ALL)
        {
            if(strlen(WelcomeString0) == 0 && strlen(WelcomeString1) == 0)
            {
                strcpy(WelcomeString0, "WELCOME");
                strcpy(WelcomeString1, WelcomeString2);
            }
            else if(strlen(WelcomeString0) == 0 || strlen(WelcomeString1) == 0)
            {
                if(strlen(WelcomeString0) == 0)
                {
                    strcpy(WelcomeString0, WelcomeString1);
                }
                strcpy(WelcomeString1, WelcomeString2);
            }
        }
        else if(gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_MESSAGE)
        {
            if(strlen(WelcomeString0) == 0)
            {
                strcpy(WelcomeString0, "WELCOME");
            }

            if(strlen(WelcomeString1) == 0)
            {
                strcpy(WelcomeString1, "BIENVENUE");
            }
        }

        UI_PrintString(WelcomeString0, 0, 127, 0, 10);
        UI_PrintString(WelcomeString1, 0, 127, 2, 10);

#ifdef ENABLE_FEAT_F4HWN
      //  UI_PrintStringSmallNormal(Edition, 0, 128, 4);
        UI_PrintString(Edition, 0, 127, 4, 10);

        //*******************ЛИНИИ-LINES***************** */


        for (uint8_t y = 47; y <= 57; y += 2) {
            UI_DrawLineBuffer(gFrameBuffer, 30, y, 30, y, 1); // Левая вертикальная пунктирная(X = 30)
        }
        for (uint8_t y = 47; y <= 57; y += 2) {
            UI_DrawLineBuffer(gFrameBuffer, 94, y, 94, y, 1);  // Правая вертикальная пунктирная (X = 90)
        }
        // Горизонтальные у тебя уже были правильные, 
        // for (uint8_t i = 105; i <= 127; i += 2) {
        //     UI_DrawLineBuffer(gFrameBuffer, i, 38, i, 38, 1); // Hory X
        // }
        // for (uint8_t i = 0; i <= 22; i += 2) {
        //     UI_DrawLineBuffer(gFrameBuffer, i, 38, i, 38, 1); // Hory X
        // }
        for (uint8_t i = 0; i <= 127; i += 2) {
            UI_DrawLineBuffer(gFrameBuffer, i, 15, i, 15, 1); // Hory X
        }
                for (uint8_t i = 0; i <= 127; i += 2) {
            UI_DrawLineBuffer(gFrameBuffer, i, 30, i, 30, 1); // Hory X
        }

 GUI_DisplaySmallest("BETA", 5, 46, false, true);
 GUI_DisplaySmallest("TEST", 108, 46, false, true);



        sprintf(WelcomeString3, "%s", Version);
        UI_PrintStringSmallBold(WelcomeString3, 0, 127, 6);

#else
        UI_PrintStringSmallNormal(Version, 0, 127, 6);
#endif

        
        ST7565_BlitFullScreen();

        #ifdef ENABLE_FEAT_F4HWN_SCREENSHOT
            getScreenShot(true);
        #endif
    }
}
