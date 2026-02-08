 

#include <stdint.h>
#include <string.h>
#include <stdio.h>      
#include "audio.h"
#include "board.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "version.h"

#ifdef ENABLE_FEAT_F4HWN
    #ifdef ENABLE_FMRADIO
        #include "app/action.h"
        #include "ui/ui.h"
    #endif
    #ifdef ENABLE_SPECTRUM
        #include "app/spectrum.h"
    #endif
    #include "app/chFrScanner.h"
#endif

#include "app/app.h"
#include "app/dtmf.h"

#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/py25q16.h"
#ifdef ENABLE_UART
    #include "driver/uart.h"
#endif
#ifdef ENABLE_USB
#include "driver/vcp.h"
#endif
#include "helper/battery.h"
#include "helper/boot.h"

#include "ui/lock.h"
#include "ui/welcome.h"
#include "ui/menu.h"

#include "external/printf/printf.h"

void _putchar(__attribute__((unused)) char c)
{

#ifdef ENABLE_UART
    UART_Send((uint8_t *)&c, 1);
#endif

}

void Main(void)
{
    SYSTICK_Init();
    BOARD_Init();

    boot_counter_10ms = 250;    

#ifdef ENABLE_UART
    UART_Init();
    UART_Send(UART_Version, strlen(UART_Version));
#endif
#ifdef ENABLE_USB
    VCP_Init();
#endif

    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
    memset(gFrameBufferOld, 1, sizeof(gFrameBufferOld));
    memset(gStatusLine,  0, sizeof(gStatusLine));
    memset(gStatusLineOld,  1, sizeof(gStatusLineOld));

    memset(gDTMF_String, '-', sizeof(gDTMF_String));
    gDTMF_String[sizeof(gDTMF_String) - 1] = 0;

    BK4819_Init();

    BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage, &gBatteryCurrent);

    SETTINGS_InitEEPROM();

    #ifdef ENABLE_FEAT_F4HWN
        gDW = gEeprom.DUAL_WATCH;
        gCB = gEeprom.CROSS_BAND_RX_TX;
    #endif

    SETTINGS_WriteBuildOptions();
    SETTINGS_LoadCalibration();

    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

    RADIO_SelectVfos();

    RADIO_SetupRegisters(true);

    for (unsigned int i = 0; i < ARRAY_SIZE(gBatteryVoltages); i++)
        BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);

    BATTERY_GetReadings(false);

    BOOT_Mode_t  BootMode = BOOT_GetMode();

    if (BootMode == BOOT_MODE_F_LOCK)
    {

        gF_LOCK = true;             
        #ifdef ENABLE_FEAT_F4HWN
            gEeprom.KEY_LOCK = 0;
            SETTINGS_SaveSettings();
            #ifndef ENABLE_VOX
                gMenuCursor = 67;  
            #else
                gMenuCursor = 68;  
            #endif

            #ifdef ENABLE_NOAA
                gMenuCursor += 1;  
            #endif
  
            gSubMenuSelection = gSetting_F_LOCK;
        #endif
    }

    gMenuListCount = 0;
    while (MenuList[gMenuListCount].name[0] != '\0') {
        gMenuListCount++;  
    }

    if (GPIO_IsPttPressed() ||
         KEYBOARD_Poll() != KEY_INVALID ||
         BootMode != BOOT_MODE_NORMAL)
    {    
        UI_DisplayReleaseKeys();
        BACKLIGHT_TurnOn();

        for (int i = 0; i < 50;)
        {
            i = (!GPIO_IsPttPressed() && KEYBOARD_Poll() == KEY_INVALID) ? i + 1 : 0;
            SYSTEM_DelayMs(10);
        }
        gKeyReading0 = KEY_INVALID;
        gKeyReading1 = KEY_INVALID;
        gDebounceCounter = 0;
    }

    if (!gChargingWithTypeC && gBatteryDisplayLevel == 0)
    {
        FUNCTION_Select(FUNCTION_POWER_SAVE);

        if (gEeprom.BACKLIGHT_TIME < 61)  
            BACKLIGHT_TurnOff();     
        else
            BACKLIGHT_TurnOn();      

        gReducedService = true;
    }
    else
    {
        UI_DisplayWelcome();

        BACKLIGHT_TurnOn();

#ifdef ENABLE_FEAT_F4HWN
        if (gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_NONE && gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_SOUND)
#else
        if (gEeprom.POWER_ON_DISPLAY_MODE != POWER_ON_DISPLAY_MODE_NONE)
#endif
        {    
            while (boot_counter_10ms > 0)
            {
                if (KEYBOARD_Poll() != KEY_INVALID)
                {    
                    boot_counter_10ms = 0;
                    break;
                }
            }
            RADIO_SetupRegisters(true);
        }

#ifdef ENABLE_PWRON_PASSWORD
        if (gEeprom.POWER_ON_PASSWORD < 1000000)
        {
            bIsInLockScreen = true;
            UI_DisplayLock();
            bIsInLockScreen = false;

            for (int i = 0; i < 50;)
            {
                i = (GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) && KEYBOARD_Poll() == KEY_INVALID) ? i + 1 : 0;
                SYSTEM_DelayMs(10);
            }
            gKeyReading0 = KEY_INVALID;
            gKeyReading1 = KEY_INVALID;
            gDebounceCounter = 0;
        }
#endif

        BOOT_ProcessMode(BootMode);

        gUpdateStatus = true;

#ifdef ENABLE_VOICE
        {
            uint8_t Channel;

            AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);

            Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];
            if (IS_MR_CHANNEL(Channel))
            {
                AUDIO_SetVoiceID(1, VOICE_ID_CHANNEL_MODE);
                AUDIO_SetDigitVoice(2, Channel + 1);
            }
            else if (IS_FREQ_CHANNEL(Channel))
                AUDIO_SetVoiceID(1, VOICE_ID_FREQUENCY_MODE);

            AUDIO_PlaySingleVoice(0);
        }
#endif

#ifdef ENABLE_NOAA
        RADIO_ConfigureNOAA();
#endif
    }

    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        if (gEeprom.CURRENT_STATE == 2 || gEeprom.CURRENT_STATE == 5) {
            gScanRangeStart = gScanRangeStart ? 0 : gTxVfo->pRX->Frequency;
            gScanRangeStop = gEeprom.VfoInfo[!gEeprom.TX_VFO].freq_config_RX.Frequency;
            if (gScanRangeStart > gScanRangeStop) {
                SWAP(gScanRangeStart, gScanRangeStop);
            }
        }

        if (gEeprom.CURRENT_STATE == 1) {
            gEeprom.SCAN_LIST_DEFAULT = gEeprom.CURRENT_LIST;
        }

        if (gEeprom.CURRENT_STATE == 1 || gEeprom.CURRENT_STATE == 2) {
            CHFRSCANNER_Start(true, SCAN_FWD);
        }
        #ifdef ENABLE_FMRADIO
        else if (gEeprom.CURRENT_STATE == 3) {
            ACTION_FM();
            GUI_SelectNextDisplay(gRequestDisplayScreen);
        }
        #endif
        #ifdef ENABLE_SPECTRUM
        else if (gEeprom.CURRENT_STATE == 4 || gEeprom.CURRENT_STATE == 5) {
            APP_RunSpectrum();
        }
        #endif
    #endif

    while (true)
    {
         
        APP_Update();

        if (gNextTimeslice)
        {
             
            APP_TimeSlice10ms();

            if (gNextTimeslice_500ms)
            {
                APP_TimeSlice500ms();
            }
        }
    }
}
