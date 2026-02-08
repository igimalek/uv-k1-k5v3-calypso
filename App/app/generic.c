

#include <string.h>

#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"

#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif

#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "audio.h"
#include "driver/keyboard.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void GENERIC_Key_F(bool bKeyPressed, bool bKeyHeld)
{
    if (gInputBoxIndex > 0) {
        if (!bKeyHeld && bKeyPressed) 
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    if (bKeyHeld || !bKeyPressed) { 
        if (bKeyHeld || bKeyPressed) { 
            if (!bKeyHeld) 
                return;

            if (!bKeyPressed) 
                return;

            COMMON_KeypadLockToggle();
        }
        else { 
#ifdef ENABLE_FMRADIO
            if ((gFmRadioMode || gScreenToDisplay != DISPLAY_MAIN) && gScreenToDisplay != DISPLAY_FM)
                return;
#else
            if (gScreenToDisplay != DISPLAY_MAIN)
                return;
#endif

            gWasFKeyPressed = !gWasFKeyPressed; 

            if (gWasFKeyPressed)
                gKeyInputCountdown = key_input_timeout_500ms;

#ifdef ENABLE_VOICE
            if (!gWasFKeyPressed)
                gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
            gUpdateStatus = true;
        }
    }
    else { 
#ifdef ENABLE_FMRADIO
        if (gScreenToDisplay != DISPLAY_FM)
#endif
        {
            gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            return;
        }

#ifdef ENABLE_FMRADIO
        if (gFM_ScanState == FM_SCAN_OFF) { 
            gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            return;
        }
#endif
        gBeepToPlay     = BEEP_440HZ_500MS;
        gPttWasReleased = true;
    }
}

void GENERIC_Key_PTT(bool bKeyPressed)
{
    gInputBoxIndex = 0;

    if (!bKeyPressed || SerialConfigInProgress())
    {   
        if (gCurrentFunction == FUNCTION_TRANSMIT) {    
            
            if (gFlagEndTransmission) {
                FUNCTION_Select(FUNCTION_FOREGROUND);
            }
            else {
                APP_EndTransmission();

                if (gEeprom.REPEATER_TAIL_TONE_ELIMINATION == 0)
                    FUNCTION_Select(FUNCTION_FOREGROUND);
                else
                    gRTTECountdown_10ms = gEeprom.REPEATER_TAIL_TONE_ELIMINATION * 10;
            }

            gFlagEndTransmission = false;
#ifdef ENABLE_VOX
            gVOX_NoiseDetected = false;
#endif
            RADIO_SetVfoState(VFO_STATE_NORMAL);

            if (gScreenToDisplay != DISPLAY_MENU)     
                gRequestDisplayScreen = DISPLAY_MAIN;
        }

        return;
    }

    

    if (SCANNER_IsScanning()) { 
        SCANNER_Stop(); 
        goto cancel_tx;
    }

    if (gScanStateDir != SCAN_OFF) {    
        CHFRSCANNER_Stop(); 
        goto cancel_tx;
    }

#ifdef ENABLE_FMRADIO
    if (gFM_ScanState != FM_SCAN_OFF) { 
        FM_PlayAndUpdate();
#ifdef ENABLE_VOICE
        gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
        gRequestDisplayScreen = DISPLAY_FM;
        goto cancel_tx;
    }
#endif

#ifdef ENABLE_FMRADIO
    if (gScreenToDisplay == DISPLAY_FM)
        goto start_tx;  
#endif

    if (gCurrentFunction == FUNCTION_TRANSMIT && gRTTECountdown_10ms == 0) {
        gInputBoxIndex = 0;
        return;
    }

    if (gScreenToDisplay != DISPLAY_MENU)     
        gRequestDisplayScreen = DISPLAY_MAIN;

    if (!gDTMF_InputMode && gDTMF_InputBox_Index == 0)
        goto start_tx;  

    

    if (gDTMF_InputBox_Index > 0 || gDTMF_PreviousIndex > 0) { 
        if (gDTMF_InputBox_Index == 0 && gDTMF_PreviousIndex > 0)
            gDTMF_InputBox_Index = gDTMF_PreviousIndex;           

        if (gDTMF_InputBox_Index < sizeof(gDTMF_InputBox))
            gDTMF_InputBox[gDTMF_InputBox_Index] = 0;             

#ifdef ENABLE_DTMF_CALLING
        
        
        if (gDTMF_InputBox_Index == 3 && gTxVfo->DTMF_DECODING_ENABLE > 0)
            gDTMF_CallMode = DTMF_CheckGroupCall(gDTMF_InputBox, 3);
        else
            gDTMF_CallMode = DTMF_CALL_MODE_DTMF;

        gDTMF_State      = DTMF_STATE_0;
#endif
        
        gDTMF_PreviousIndex = gDTMF_InputBox_Index;
        strcpy(gDTMF_String, gDTMF_InputBox);
        gDTMF_ReplyState = DTMF_REPLY_ANI;
    }

    DTMF_clear_input_box();

start_tx:
    
    gFlagPrepareTX = true;
    goto done;

cancel_tx:
    if (gPttIsPressed) {
        gPttWasPressed = true;
    }

done:
    gPttDebounceCounter = 0;
    if (gScreenToDisplay != DISPLAY_MENU
#ifdef ENABLE_FMRADIO
        && gRequestDisplayScreen != DISPLAY_FM
#endif
    ) {
        
        gRequestDisplayScreen = DISPLAY_MAIN;
    }

    gUpdateStatus  = true;
    gUpdateDisplay = true;
}
