  

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "rega.h"
#include "radio.h"
#include "action.h"
#include "misc.h"
#include "settings.h"
#include "functions.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/gpio.h"
#include "bsp/dp32g030/gpio.h"
#include "ui/helper.h"
#include "ui/ui.h"


const uint16_t rega_alarm_tones[5] = {
    1160,    
    1060,    
    1400,    
    1060,    
    1400,    
};
const uint16_t rega_test_tones[5] = {
    1160,   
    1060,   
    1260,   
    2400,   
    1060,   
};

  
char rega_message[16];

  
void ACTION_RegaAlarm()
{
    const char message[16] = "REGA Alarm";
    REGA_TransmitZvei(rega_alarm_tones,message);
}

  
void ACTION_RegaTest()
{
    const char message[16] = "REGA Test";
    REGA_TransmitZvei(rega_test_tones,message);
}

  
void UI_DisplayREGA()
{
    UI_DisplayClear();
    UI_DisplayPopup(rega_message);
    ST7565_BlitFullScreen();
}


void REGA_TransmitZvei(const uint16_t tones[],const char message[])
{
      
    strncpy(rega_message,message,16);
      
    gScreenToDisplay = DISPLAY_REGA;

      
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
    SYSTEM_DelayMs(70);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    SYSTEM_DelayMs(50);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
    SYSTEM_DelayMs(70);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);

      
    gEeprom.TX_VFO = 0;
    gEeprom.ScreenChannel[gEeprom.TX_VFO] = gEeprom.FreqChannel[gEeprom.TX_VFO];
    gRxVfo = gTxVfo;
    gRequestSaveVFO = true;
    gVfoConfigureMode = VFO_CONFIGURE_RELOAD;

      
    gTxVfo->freq_config_RX.Frequency = REGA_FREQUENCY;
    gTxVfo->freq_config_TX           = gTxVfo->freq_config_RX;
    gTxVfo->TX_OFFSET_FREQUENCY = 0;

      
    gTxVfo->Modulation = MODULATION_FM;
    gTxVfo->CHANNEL_BANDWIDTH = BK4819_FILTER_BW_NARROW;

      
    gTxVfo->freq_config_TX.CodeType = CODE_TYPE_CONTINUOUS_TONE;
    gTxVfo->freq_config_TX.Code     = REGA_CTCSS_FREQ_INDEX;
    BK4819_SetCTCSSFrequency(CTCSS_Options[gTxVfo->freq_config_TX.Code]);
      
    gTxVfo->freq_config_RX.CodeType = CODE_TYPE_OFF;

      
    gTxVfo->OUTPUT_POWER = OUTPUT_POWER_HIGH;

      
    gEeprom.KEY_LOCK = true;

    gRequestSaveChannel = 1;
    gRequestSaveSettings = true;

      
    BK4819_RX_TurnOn();

      
    RADIO_PrepareCssTX();
    FUNCTION_Select(FUNCTION_TRANSMIT);

      
    SYSTEM_DelayMs(ZVEI_PRE_LENGTH_MS);

      
    for (int i = 0; i < ZVEI_NUM_TONES; i++)
    {
        BK4819_PlaySingleTone(tones[i], ZVEI_TONE_LENGTH_MS, 100, true);
        SYSTEM_DelayMs(ZVEI_PAUSE_LENGTH_MS);
    }

      
    SYSTEM_DelayMs(ZVEI_POST_LENGTH_MS);

      
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
    SYSTEM_DelayMs(70);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    SYSTEM_DelayMs(70);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
    SYSTEM_DelayMs(70);
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);

      
    gRequestDisplayScreen = DISPLAY_MAIN;
}
