 

#include <stdint.h>
#include <stdio.h>

#include "settings.h"

#include "audio.h"

#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"


#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define PIN_CSN GPIO_MAKE_PIN(GPIOF, LL_GPIO_PIN_9)
#define PIN_SCL GPIO_MAKE_PIN(GPIOB, LL_GPIO_PIN_8)
#define PIN_SDA GPIO_MAKE_PIN(GPIOB, LL_GPIO_PIN_9)

static const uint16_t FSK_RogerTable[7] = {0xF1A2, 0x7446, 0x61A4, 0x6544, 0x4E8A, 0xE044, 0xEA84};

static const uint8_t DTMF_TONE1_GAIN = 65;
static const uint8_t DTMF_TONE2_GAIN = 93;

static uint16_t gBK4819_GpioOutState;

bool gRxIdleMode;

static inline void CS_Assert()
{
    GPIO_ResetOutputPin(PIN_CSN);
}

static inline void CS_Release()
{
    GPIO_SetOutputPin(PIN_CSN);
}

static inline void SCL_Reset()
{
    GPIO_ResetOutputPin(PIN_SCL);
}

static inline void SCL_Set()
{
    GPIO_SetOutputPin(PIN_SCL);
}

static inline void SDA_Reset()
{
    GPIO_ResetOutputPin(PIN_SDA);
}

static inline void SDA_Set()
{
    GPIO_SetOutputPin(PIN_SDA);
}

static inline void SDA_SetDir(bool Output)
{
    LL_GPIO_SetPinMode(GPIO_PORT(PIN_SDA), GPIO_PIN_MASK(PIN_SDA), Output ? LL_GPIO_MODE_OUTPUT : LL_GPIO_MODE_INPUT);
}

static inline uint32_t SDA_ReadInput()
{
    return GPIO_IsInputPinSet(PIN_SDA) ? 1 : 0;
}

__inline uint16_t scale_freq(const uint16_t freq)
{
 
    return (((uint32_t)freq * 1353245u) + (1u << 16)) >> 17;    
}

void BK4819_Init(void)
{
    CS_Release();
    SCL_Set();
    SDA_Set();

    BK4819_WriteRegister(BK4819_REG_00, 0x8000);
    BK4819_WriteRegister(BK4819_REG_00, 0x0000);

    BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
    BK4819_WriteRegister(BK4819_REG_36, 0x0022);

    BK4819_InitAGC(false);
    BK4819_SetAGC(true);

    BK4819_WriteRegister(BK4819_REG_19, 0b0001000001000001);    

    BK4819_WriteRegister(BK4819_REG_7D, 0xE940);


    BK4819_WriteRegister(BK4819_REG_48,  
        (11u << 12) |      
        ( 0u << 10) |      
        (58u <<  4) |      
        ( 8u <<  0));      

#if 1
    const uint8_t dtmf_coeffs[] = {111, 107, 103, 98, 80, 71, 58, 44, 65, 55, 37, 23, 228, 203, 181, 159};
    for (unsigned int i = 0; i < ARRAY_SIZE(dtmf_coeffs); i++)
        BK4819_WriteRegister(BK4819_REG_09, (i << 12) | dtmf_coeffs[i]);
#else
     
    BK4819_WriteRegister(BK4819_REG_09, 0x006F);   
    BK4819_WriteRegister(BK4819_REG_09, 0x106B);   
    BK4819_WriteRegister(BK4819_REG_09, 0x2067);   
    BK4819_WriteRegister(BK4819_REG_09, 0x3062);   
    BK4819_WriteRegister(BK4819_REG_09, 0x4050);   
    BK4819_WriteRegister(BK4819_REG_09, 0x5047);   
    BK4819_WriteRegister(BK4819_REG_09, 0x603A);   
    BK4819_WriteRegister(BK4819_REG_09, 0x702C);   
    BK4819_WriteRegister(BK4819_REG_09, 0x8041);   
    BK4819_WriteRegister(BK4819_REG_09, 0x9037);   
    BK4819_WriteRegister(BK4819_REG_09, 0xA025);   
    BK4819_WriteRegister(BK4819_REG_09, 0xB017);   
    BK4819_WriteRegister(BK4819_REG_09, 0xC0E4);   
    BK4819_WriteRegister(BK4819_REG_09, 0xD0CB);   
    BK4819_WriteRegister(BK4819_REG_09, 0xE0B5);   
    BK4819_WriteRegister(BK4819_REG_09, 0xF09F);   
#endif

    BK4819_WriteRegister(BK4819_REG_1F, 0x5454);
    BK4819_WriteRegister(BK4819_REG_3E, 0xA037);

    gBK4819_GpioOutState = 0x9000;

    BK4819_WriteRegister(BK4819_REG_33, 0x9000);
    BK4819_WriteRegister(BK4819_REG_3F, 0);
}

static uint16_t BK4819_ReadU16(void)
{
    unsigned int i;
    uint16_t     Value;

    SDA_SetDir(false);
    SYSTICK_DelayUs(1);
    Value = 0;
    for (i = 0; i < 16; i++)
    {
        Value <<= 1;
        Value |= SDA_ReadInput();
        SCL_Set();
        SYSTICK_DelayUs(1);
        SCL_Reset();
        SYSTICK_DelayUs(1);
    }
    SDA_SetDir(true);

    return Value;
}

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register)
{
    uint16_t Value;

    CS_Release();
    SCL_Reset();

    SYSTICK_DelayUs(1);

    CS_Assert();
    BK4819_WriteU8(Register | 0x80);
    Value = BK4819_ReadU16();
    CS_Release();

    SYSTICK_DelayUs(1);

    SCL_Set();
    SDA_Set();

    return Value;
}

void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data)
{
    CS_Release();
    SCL_Reset();

    SYSTICK_DelayUs(1);

    CS_Assert();
    BK4819_WriteU8(Register);

    SYSTICK_DelayUs(1);

    BK4819_WriteU16(Data);

    SYSTICK_DelayUs(1);

    CS_Release();

    SYSTICK_DelayUs(1);

    SCL_Set();
    SDA_Set();
}

void BK4819_WriteU8(uint8_t Data)
{
    unsigned int i;

    SCL_Reset();
    for (i = 0; i < 8; i++)
    {
        if ((Data & 0x80) == 0)
            SDA_Reset();
        else
            SDA_Set();

        SYSTICK_DelayUs(1);
        SCL_Set();
        SYSTICK_DelayUs(1);

        Data <<= 1;

        SCL_Reset();
        SYSTICK_DelayUs(1);
    }
}

void BK4819_WriteU16(uint16_t Data)
{
    unsigned int i;

    SCL_Reset();
    for (i = 0; i < 16; i++)
    {
        if ((Data & 0x8000) == 0)
            SDA_Reset();
        else
            SDA_Set();

        SYSTICK_DelayUs(1);
        SCL_Set();

        Data <<= 1;

        SYSTICK_DelayUs(1);
        SCL_Reset();
        SYSTICK_DelayUs(1);
    }
}

void BK4819_SetAGC(bool enable)
{
    uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);
    if(!(regVal & (1 << 15)) == enable)
        return;

    BK4819_WriteRegister(BK4819_REG_7E, (regVal & ~(1 << 15) & ~(0b111 << 12))
        | (!enable << 15)    
        | (3u << 12)        
    );


}

void BK4819_InitAGC(bool amModulation)
{


    BK4819_WriteRegister(BK4819_REG_13, 0x03BE);   
    BK4819_WriteRegister(BK4819_REG_12, 0x037B);   
    BK4819_WriteRegister(BK4819_REG_11, 0x027B);   
    BK4819_WriteRegister(BK4819_REG_10, 0x007A);   
    if(amModulation) {
        BK4819_WriteRegister(BK4819_REG_14, 0x0000);
        BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (50 << 7) | (32 << 0));
    }
    else{
        BK4819_WriteRegister(BK4819_REG_14, 0x0019);   
        BK4819_WriteRegister(BK4819_REG_49, (0 << 14) | (84 << 7) | (56 << 0));  
    }

    BK4819_WriteRegister(BK4819_REG_7B, 0x8420);

}

int8_t BK4819_GetRxGain_dB(void)
{
    union {
        struct {
            uint16_t pga:3;
            uint16_t mixer:2;
            uint16_t lna:3;
            uint16_t lnaS:2;
        };
        uint16_t __raw;
    } agcGainReg;

    union {
        struct {
            uint16_t _ : 5;
            uint16_t agcSigStrength : 7;
            int16_t gainIdx : 3;
            uint16_t agcEnab : 1;
        };
        uint16_t __raw;
    } reg7e;

    reg7e.__raw = BK4819_ReadRegister(BK4819_REG_7E);
    uint8_t gainAddr = reg7e.gainIdx < 0 ? BK4819_REG_14 : BK4819_REG_10 + reg7e.gainIdx;
    agcGainReg.__raw = BK4819_ReadRegister(gainAddr);
    int8_t lnaShortTab[] = {-28, -24, -19, 0};
    int8_t lnaTab[] = {-24, -19, -14, -9, -6, -4, -2, 0};
    int8_t mixerTab[] = {-8, -6, -3, 0};
    int8_t pgaTab[] = {-33, -27, -21, -15, -9, -6, -3, 0};
    return lnaShortTab[agcGainReg.lnaS] + lnaTab[agcGainReg.lna] + mixerTab[agcGainReg.mixer] + pgaTab[agcGainReg.pga];
}

int16_t BK4819_GetRSSI_dBm(void)
{
    uint16_t rssi = BK4819_GetRSSI();
    return (rssi / 2) - 160; 
}

void BK4819_ToggleGpioOut(BK4819_GPIO_PIN_t Pin, bool bSet)
{
    if (bSet)
        gBK4819_GpioOutState |=  (0x40u >> Pin);
    else
        gBK4819_GpioOutState &= ~(0x40u >> Pin);

    BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SetCDCSSCodeWord(uint32_t CodeWord)
{


    BK4819_WriteRegister(BK4819_REG_51,
        BK4819_REG_51_ENABLE_CxCSS         |
        BK4819_REG_51_GPIO6_PIN2_NORMAL    |
        BK4819_REG_51_TX_CDCSS_POSITIVE    |
        BK4819_REG_51_MODE_CDCSS           |
        BK4819_REG_51_CDCSS_23_BIT         |
        BK4819_REG_51_1050HZ_NO_DETECTION  |
        BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
        BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
        (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));


    BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | 2775u);


    BK4819_WriteRegister(BK4819_REG_08, (0u << 15) | ((CodeWord >>  0) & 0x0FFF));  
    BK4819_WriteRegister(BK4819_REG_08, (1u << 15) | ((CodeWord >> 12) & 0x0FFF));  
}

void BK4819_SetCTCSSFrequency(uint32_t FreqControlWord)
{


    uint16_t Config;
    if (FreqControlWord == 2625)
    {    


        Config = 0x944A;    
    }
    else
    {    


        Config = 0x904A;    
    }
    BK4819_WriteRegister(BK4819_REG_51, Config);


    BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | (((FreqControlWord * 206488u) + 50000u) / 100000u));    
}

 
void BK4819_SetTailDetection(const uint32_t freq_10Hz)
{


    BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC2 | ((253910 + (freq_10Hz / 2)) / freq_10Hz));   
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold, uint16_t VoxDisableThreshold)
{


    const uint16_t REG_31_Value = BK4819_ReadRegister(BK4819_REG_31);

     
    BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (VoxEnableThreshold & 0x07FF));

     
    BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (VoxDisableThreshold & 0x07FF));

     
    BK4819_WriteRegister(BK4819_REG_7A, 0x289A);  

     
    BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | (1u << 2));     
}

void BK4819_SetFilterBandwidth(const BK4819_FilterBandwidth_t Bandwidth, const bool weak_no_different)
{


    uint16_t val = 0;
    switch (Bandwidth)
    {
        default:
        case BK4819_FILTER_BW_WIDE:  
            if (weak_no_different) {
                 
                val = 0x3628;  
            } else {
                 
                val = 0x3428;  
            }
            break;

        case BK4819_FILTER_BW_NARROW:    
            if (weak_no_different) {
                 
                val = 0x3648;  
            } else {
                 
                val = 0x3448;  
            }
            break;

        case BK4819_FILTER_BW_NARROWER:  
            if (weak_no_different) {
                 
                val = 0x1348;  
            } else {
                 
                val = 0x1148;  
            }
            break;

         
    }

    BK4819_WriteRegister(BK4819_REG_43, val);
}

void BK4819_SetupPowerAmplifier(const uint8_t bias, const uint32_t frequency)
{


    const uint8_t gain   = (frequency < 28000000) ? (1u << 3) | (0u << 0) : (4u << 3) | (2u << 0);
    const uint8_t enable = 1;
    BK4819_WriteRegister(BK4819_REG_36, (bias << 8) | (enable << 7) | (gain << 0));
}

void BK4819_SetFrequency(uint32_t Frequency)
{
    BK4819_WriteRegister(BK4819_REG_38, (Frequency >>  0) & 0xFFFF);
    BK4819_WriteRegister(BK4819_REG_39, (Frequency >> 16) & 0xFFFF);
}

void BK4819_SetupSquelch(
        uint8_t SquelchOpenRSSIThresh,
        uint8_t SquelchCloseRSSIThresh,
        uint8_t SquelchOpenNoiseThresh,
        uint8_t SquelchCloseNoiseThresh,
        uint8_t SquelchCloseGlitchThresh,
        uint8_t SquelchOpenGlitchThresh)
{


    BK4819_WriteRegister(BK4819_REG_70, 0);


    BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | SquelchCloseGlitchThresh);


    BK4819_WriteRegister(BK4819_REG_4E,   

         
    (1u << 14) |                   
    (5u << 11) |                   
    (6u <<  9) |                   
    SquelchOpenGlitchThresh);      


    BK4819_WriteRegister(BK4819_REG_4F, ((uint16_t)SquelchCloseNoiseThresh << 8) | SquelchOpenNoiseThresh);


    BK4819_WriteRegister(BK4819_REG_78, ((uint16_t)SquelchOpenRSSIThresh   << 8) | SquelchCloseRSSIThresh);

    BK4819_SetAF(BK4819_AF_MUTE);

    BK4819_RX_TurnOn();
}

void BK4819_SetAF(BK4819_AF_Type_t AF)
{


    BK4819_WriteRegister(BK4819_REG_47, (6u << 12) | (AF << 8) | (1u << 6));
}

void BK4819_SetRegValue(RegisterSpec s, uint16_t v) {
  uint16_t reg = BK4819_ReadRegister(s.num);
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
}

void BK4819_RX_TurnOn(void)
{


    BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);   

     
    BK4819_WriteRegister(BK4819_REG_30, 0);


    BK4819_WriteRegister(BK4819_REG_30,
        BK4819_REG_30_ENABLE_VCO_CALIB |
        BK4819_REG_30_DISABLE_UNKNOWN |
        BK4819_REG_30_ENABLE_RX_LINK |
        BK4819_REG_30_ENABLE_AF_DAC |
        BK4819_REG_30_ENABLE_DISC_MODE |
        BK4819_REG_30_ENABLE_PLL_VCO |
        BK4819_REG_30_DISABLE_PA_GAIN |
        BK4819_REG_30_DISABLE_MIC_ADC |
        BK4819_REG_30_DISABLE_TX_DSP |
        BK4819_REG_30_ENABLE_RX_DSP );
}

void BK4819_PickRXFilterPathBasedOnFrequency(uint32_t Frequency)
{
    if (Frequency < 28000000)
    {    
        BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, true);
        BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, false);
    }
    else
    if (Frequency == 0xFFFFFFFF)
    {    
        BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, false);
        BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, false);
    }
    else
    {    
        BK4819_ToggleGpioOut(BK4819_GPIO4_PIN32_VHF_LNA, false);
        BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31_UHF_LNA, true);
    }
}

void BK4819_DisableScramble(void)
{
    const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
    BK4819_WriteRegister(BK4819_REG_31, Value & ~(1u << 1));
}

void BK4819_EnableScramble(uint8_t Type)
{
    const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
    BK4819_WriteRegister(BK4819_REG_31, Value | (1u << 1));

    BK4819_WriteRegister(BK4819_REG_71, 0x68DC + (Type * 1032));    
}

bool BK4819_CompanderEnabled(void)
{
    return (BK4819_ReadRegister(BK4819_REG_31) & (1u << 3)) ? true : false;
}

void BK4819_SetCompander(const unsigned int mode)
{


    const uint16_t r31 = BK4819_ReadRegister(BK4819_REG_31);

    if (mode == 0)
    {    
        BK4819_WriteRegister(BK4819_REG_31, r31 & ~(1u << 3));
        return;
    }


    const uint16_t compress_ratio    = (mode == 1 || mode >= 3) ? 2 : 0;   
    const uint16_t compress_0dB      = 86;
    const uint16_t compress_noise_dB = 64;
 
    BK4819_WriteRegister(BK4819_REG_29,  
        (compress_ratio    << 14) |
        (compress_0dB      <<  7) |
        (compress_noise_dB <<  0));


    const uint16_t expand_ratio    = (mode >= 2) ? 1 : 0;    
    const uint16_t expand_0dB      = 86;
    const uint16_t expand_noise_dB = 56;
 
    BK4819_WriteRegister(BK4819_REG_28,  
        (expand_ratio    << 14) |
        (expand_0dB      <<  7) |
        (expand_noise_dB <<  0));

     
    BK4819_WriteRegister(BK4819_REG_31, r31 | (1u << 3));
}

void BK4819_DisableVox(void)
{
    const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
    BK4819_WriteRegister(BK4819_REG_31, Value & 0xFFFB);
}

void BK4819_DisableDTMF(void)
{
    BK4819_WriteRegister(BK4819_REG_24, 0);
}

void BK4819_EnableDTMF(void)
{
     
    BK4819_WriteRegister(BK4819_REG_21, 0x06D8);         


    const uint16_t threshold = 130;    
    BK4819_WriteRegister(BK4819_REG_24,                       
        (1u        << BK4819_REG_24_SHIFT_UNKNOWN_15) |
        (threshold << BK4819_REG_24_SHIFT_THRESHOLD)  |       
        (1u        << BK4819_REG_24_SHIFT_UNKNOWN_6)  |
                      BK4819_REG_24_ENABLE            |
                      BK4819_REG_24_SELECT_DTMF       |
        (15u       << BK4819_REG_24_SHIFT_MAX_SYMBOLS));      
}

void BK4819_PlayTone(uint16_t Frequency, bool bTuningGainSwitch)
{
    uint16_t ToneConfig = BK4819_REG_70_ENABLE_TONE1;

    BK4819_EnterTxMute();
    BK4819_SetAF(BK4819_AF_BEEP);

    if (bTuningGainSwitch == 0)
        ToneConfig |=  96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN;
    else
        ToneConfig |= 28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN;
    BK4819_WriteRegister(BK4819_REG_70, ToneConfig);

    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_TX_DSP);

    BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

 
void BK4819_PlaySingleTone(const unsigned int tone_Hz, const unsigned int delay, const unsigned int level, const bool play_speaker)
{
    BK4819_EnterTxMute();

    if (play_speaker)
    {
        AUDIO_AudioPathOn();
        BK4819_SetAF(BK4819_AF_BEEP);
    }
    else
        BK4819_SetAF(BK4819_AF_MUTE);


    BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | ((level & 0x7f) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

    BK4819_EnableTXLink();
    SYSTEM_DelayMs(50);

    BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone_Hz));

    BK4819_ExitTxMute();
    SYSTEM_DelayMs(delay);
    BK4819_EnterTxMute();

    if (play_speaker)
    {
        AUDIO_AudioPathOff();
        BK4819_SetAF(BK4819_AF_MUTE);
    }

    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
    BK4819_ExitTxMute();
}

void BK4819_EnterTxMute(void)
{
    BK4819_WriteRegister(BK4819_REG_50, 0xBB20);
}

void BK4819_ExitTxMute(void)
{
    BK4819_WriteRegister(BK4819_REG_50, 0x3B20);
}

void BK4819_Sleep(void)
{
    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
    BK4819_WriteRegister(BK4819_REG_70, 0);
    BK4819_SetAF(BK4819_AF_MUTE);

    BK4819_ExitTxMute();

    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30,
        BK4819_REG_30_ENABLE_VCO_CALIB |
        BK4819_REG_30_ENABLE_RX_LINK   |
        BK4819_REG_30_ENABLE_AF_DAC    |
        BK4819_REG_30_ENABLE_DISC_MODE |
        BK4819_REG_30_ENABLE_PLL_VCO   |
        BK4819_REG_30_ENABLE_RX_DSP);
}

#ifdef ENABLE_AIRCOPY
    void BK4819_SetupAircopy(void)
    {
        BK4819_WriteRegister(BK4819_REG_70, 0x00E0);     
        BK4819_WriteRegister(BK4819_REG_72, 0x3065);     
        BK4819_WriteRegister(BK4819_REG_58, 0x00C1);     
                                                         
        BK4819_WriteRegister(BK4819_REG_5C, 0x5665);     
        BK4819_WriteRegister(BK4819_REG_5D, 0x4700);     
    }
#endif

void BK4819_ResetFSK(void)
{
    BK4819_WriteRegister(BK4819_REG_3F, 0x0000);         
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);         

    SYSTEM_DelayMs(30);

    BK4819_Idle();
}

void BK4819_Idle(void)
{
    BK4819_WriteRegister(BK4819_REG_30, 0x0000);
}

void BK4819_ExitBypass(void)
{
    BK4819_SetAF(BK4819_AF_MUTE);


    uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);

     
    BK4819_WriteRegister(BK4819_REG_7E, (regVal & ~(0b111 << 3))

        | (5u <<  3)        

    );
}

void BK4819_PrepareTransmit(void)
{
    BK4819_ExitBypass();
    BK4819_ExitTxMute();
    BK4819_TxOn_Beep();
}

void BK4819_TxOn_Beep(void)
{
    BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
    BK4819_WriteRegister(BK4819_REG_52, 0x028F);
    BK4819_WriteRegister(BK4819_REG_30, 0x0000);
    BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_ExitSubAu(void)
{


    BK4819_WriteRegister(BK4819_REG_51, 0x0000);
}

void BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable(void)
{
    if (gRxIdleMode)
    {
        BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
        BK4819_RX_TurnOn();
    }
}

void BK4819_EnterDTMF_TX(bool bLocalLoopback)
{
    BK4819_EnableDTMF();
    BK4819_EnterTxMute();
    BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

    BK4819_WriteRegister(BK4819_REG_70,
        BK4819_REG_70_MASK_ENABLE_TONE1                |
        (DTMF_TONE1_GAIN << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN) |
        BK4819_REG_70_MASK_ENABLE_TONE2                |
        (DTMF_TONE2_GAIN << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));

    BK4819_EnableTXLink();
}

void BK4819_ExitDTMF_TX(bool bKeep)
{
    BK4819_EnterTxMute();
    BK4819_SetAF(BK4819_AF_MUTE);
    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_DisableDTMF();
    BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
    if (!bKeep)
        BK4819_ExitTxMute();
}

void BK4819_EnableTXLink(void)
{
    BK4819_WriteRegister(BK4819_REG_30,
        BK4819_REG_30_ENABLE_VCO_CALIB |
        BK4819_REG_30_ENABLE_UNKNOWN   |
        BK4819_REG_30_DISABLE_RX_LINK  |
        BK4819_REG_30_ENABLE_AF_DAC    |
        BK4819_REG_30_ENABLE_DISC_MODE |
        BK4819_REG_30_ENABLE_PLL_VCO   |
        BK4819_REG_30_ENABLE_PA_GAIN   |
        BK4819_REG_30_DISABLE_MIC_ADC  |
        BK4819_REG_30_ENABLE_TX_DSP    |
        BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_PlayDTMF(char Code)
{

    struct DTMF_TonePair {
        uint16_t tone1;
        uint16_t tone2;
    };

    const struct DTMF_TonePair tones[] = {
        {941, 1336},
        {697, 1209},
        {697, 1336},
        {697, 1477},
        {770, 1209},
        {770, 1336},
        {770, 1477},
        {852, 1209},
        {852, 1336},
        {852, 1477},
        {697, 1633},
        {770, 1633},
        {852, 1633},
        {941, 1633},
        {941, 1209},
        {941, 1477},
    };


    const struct DTMF_TonePair *pSelectedTone = NULL;
    switch (Code)
    {
        case '0'...'9': pSelectedTone = &tones[0  + Code - '0']; break;
        case 'A'...'D': pSelectedTone = &tones[10 + Code - 'A']; break;
        case '*': pSelectedTone = &tones[14]; break;
        case '#': pSelectedTone = &tones[15]; break;
        default: pSelectedTone = NULL;
    }

    if (pSelectedTone) {
        BK4819_WriteRegister(BK4819_REG_71, (((uint32_t)pSelectedTone->tone1 * 103244) + 5000) / 10000);    
        BK4819_WriteRegister(BK4819_REG_72, (((uint32_t)pSelectedTone->tone2 * 103244) + 5000) / 10000);    
    }
}

void BK4819_PlayDTMFString(const char *pString, bool bDelayFirst, uint16_t FirstCodePersistTime, uint16_t HashCodePersistTime, uint16_t CodePersistTime, uint16_t CodeInternalTime)
{
    unsigned int i;

    if (pString == NULL)
        return;

    for (i = 0; pString[i]; i++)
    {
        uint16_t Delay;
        BK4819_PlayDTMF(pString[i]);
        BK4819_ExitTxMute();
        if (bDelayFirst && i == 0)
            Delay = FirstCodePersistTime;
        else
        if (pString[i] == '*' || pString[i] == '#')
            Delay = HashCodePersistTime;
        else
            Delay = CodePersistTime;
        SYSTEM_DelayMs(Delay);
        BK4819_EnterTxMute();
        SYSTEM_DelayMs(CodeInternalTime);
    }
}

void BK4819_TransmitTone(bool bLocalLoopback, uint32_t Frequency)
{
    BK4819_EnterTxMute();


    BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_MASK_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

    BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));

    BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

    BK4819_EnableTXLink();

    SYSTEM_DelayMs(50);

    BK4819_ExitTxMute();
}

void BK4819_GenTail(uint8_t Tail)
{


    switch (Tail)
    {
        case 0:  
            BK4819_WriteRegister(BK4819_REG_52, 0x828F);    
            break;
        case 1:  
            BK4819_WriteRegister(BK4819_REG_52, 0xA28F);    
            break;
        case 2:  
            BK4819_WriteRegister(BK4819_REG_52, 0xC28F);    
            break;
        case 3:  
            BK4819_WriteRegister(BK4819_REG_52, 0xE28F);    
            break;
        case 4:  
            BK4819_WriteRegister(BK4819_REG_07, 0x046f);    
            break;
    }
}

void BK4819_PlayCDCSSTail(void)
{
    BK4819_GenTail(0);      
    BK4819_WriteRegister(BK4819_REG_51, 0x804A);  
}

void BK4819_PlayCTCSSTail(void)
{
    #ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
        BK4819_GenTail(2);        
    #else
        BK4819_GenTail(4);        
    #endif


    BK4819_WriteRegister(BK4819_REG_51, 0x904A);  
}

uint16_t BK4819_GetRSSI(void)
{
    return BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
}

uint8_t  BK4819_GetGlitchIndicator(void)
{
    return BK4819_ReadRegister(BK4819_REG_63) & 0x00FF;
}

uint8_t  BK4819_GetExNoiceIndicator(void)
{
    return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

uint16_t BK4819_GetVoiceAmplitudeOut(void)
{
    return BK4819_ReadRegister(BK4819_REG_64);
}

uint8_t BK4819_GetAfTxRx(void)
{
    return BK4819_ReadRegister(BK4819_REG_6F) & 0x003F;
}

bool BK4819_GetFrequencyScanResult(uint32_t *pFrequency)
{
    const uint16_t High     = BK4819_ReadRegister(BK4819_REG_0D);
    const bool     Finished = (High & 0x8000) == 0;
    if (Finished)
    {
        const uint16_t Low = BK4819_ReadRegister(BK4819_REG_0E);
        *pFrequency = (uint32_t)((High & 0x7FF) << 16) | Low;
    }
    return Finished;
}

BK4819_CssScanResult_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq, uint16_t *pCtcssFreq)
{
    uint16_t Low;
    uint16_t High = BK4819_ReadRegister(BK4819_REG_69);

    if ((High & 0x8000) == 0)
    {
        Low         = BK4819_ReadRegister(BK4819_REG_6A);
        *pCdcssFreq = ((High & 0xFFF) << 12) | (Low & 0xFFF);
        return BK4819_CSS_RESULT_CDCSS;
    }

    Low = BK4819_ReadRegister(BK4819_REG_68);

    if ((Low & 0x8000) == 0)
    {
        *pCtcssFreq = ((Low & 0x1FFF) * 4843) / 10000;
        return BK4819_CSS_RESULT_CTCSS;
    }

    return BK4819_CSS_RESULT_NOT_FOUND;
}

void BK4819_DisableFrequencyScan(void)
{


    BK4819_WriteRegister(BK4819_REG_32,  
        (  0u << 14) |           
        (290u <<  1) |           
        (  0u <<  0));           
}

void BK4819_EnableFrequencyScan(void)
{


    BK4819_WriteRegister(BK4819_REG_32,  
        (  0u << 14) |           
        (290u <<  1) |           
        (  1u <<  0));           
}

void BK4819_SetScanFrequency(uint32_t Frequency)
{
    BK4819_SetFrequency(Frequency);


    BK4819_WriteRegister(BK4819_REG_51,
        BK4819_REG_51_DISABLE_CxCSS         |
        BK4819_REG_51_GPIO6_PIN2_NORMAL     |
        BK4819_REG_51_TX_CDCSS_POSITIVE     |
        BK4819_REG_51_MODE_CDCSS            |
        BK4819_REG_51_CDCSS_23_BIT          |
        BK4819_REG_51_1050HZ_NO_DETECTION   |
        BK4819_REG_51_AUTO_CDCSS_BW_DISABLE |
        BK4819_REG_51_AUTO_CTCSS_BW_DISABLE);

    BK4819_RX_TurnOn();
}

void BK4819_Disable(void)
{
    BK4819_WriteRegister(BK4819_REG_30, 0);
}

void BK4819_StopScan(void)
{
    BK4819_DisableFrequencyScan();
    BK4819_Disable();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void)
{
    return (BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F;
}

uint8_t BK4819_GetCDCSSCodeType(void)
{
    return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3u;
}

uint8_t BK4819_GetCTCShift(void)
{
    return (BK4819_ReadRegister(BK4819_REG_0C) >> 12) & 3u;
}

uint8_t BK4819_GetCTCType(void)
{
    return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3u;
}

void BK4819_SendFSKData(uint16_t *pData)
{
    unsigned int i;
    uint8_t Timeout = 200;

    SYSTEM_DelayMs(20);

    BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);
    BK4819_WriteRegister(BK4819_REG_59, 0x8068);
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);

    for (i = 0; i < 36; i++)
        BK4819_WriteRegister(BK4819_REG_5F, pData[i]);

    SYSTEM_DelayMs(20);

    BK4819_WriteRegister(BK4819_REG_59, 0x2868);

    while (Timeout-- && (BK4819_ReadRegister(BK4819_REG_0C) & 1u) == 0)
        SYSTEM_DelayMs(5);

    BK4819_WriteRegister(BK4819_REG_02, 0);

    SYSTEM_DelayMs(20);

    BK4819_ResetFSK();
}

void BK4819_PrepareFSKReceive(void)
{
    BK4819_ResetFSK();
    BK4819_WriteRegister(BK4819_REG_02, 0);
    BK4819_WriteRegister(BK4819_REG_3F, 0);
    BK4819_RX_TurnOn();
    BK4819_WriteRegister(BK4819_REG_3F, 0 | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);


    BK4819_WriteRegister(BK4819_REG_59, 0x4068);


    BK4819_WriteRegister(BK4819_REG_59, 0x3068);
}

static void BK4819_PlayRogerNormal(void)
{
    #if 0
        const uint32_t tone1_Hz = 500;
        const uint32_t tone2_Hz = 700;
    #else
         
        const uint32_t tone1_Hz = 1540;
        const uint32_t tone2_Hz = 1310;
    #endif


    BK4819_EnterTxMute();
    BK4819_SetAF(BK4819_AF_MUTE);

    BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

    BK4819_EnableTXLink();
    SYSTEM_DelayMs(50);

    BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone1_Hz));

    BK4819_ExitTxMute();
    SYSTEM_DelayMs(80);
    BK4819_EnterTxMute();

    BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone2_Hz));

    BK4819_ExitTxMute();
    SYSTEM_DelayMs(80);
    BK4819_EnterTxMute();

    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);    
}


void BK4819_PlayRogerMDC(void)
{
    struct reg_value {
        BK4819_REGISTER_t reg;
        uint16_t value;
    };

    struct reg_value RogerMDC_Configuration [] = {
        { BK4819_REG_58, 0x37C3 },   


        { BK4819_REG_72, 0x3065 },   
        { BK4819_REG_70, 0x00E0 },   
        { BK4819_REG_5D, 0x0D00 },   
        { BK4819_REG_59, 0x8068 },   
        { BK4819_REG_59, 0x0068 },   
        { BK4819_REG_5A, 0x5555 },   
        { BK4819_REG_5B, 0x55AA },   
        { BK4819_REG_5C, 0xAA30 },   
    };

    BK4819_SetAF(BK4819_AF_MUTE);

    for (unsigned int i = 0; i < ARRAY_SIZE(RogerMDC_Configuration); i++) {
        BK4819_WriteRegister(RogerMDC_Configuration[i].reg, RogerMDC_Configuration[i].value);
    }

     
    for (unsigned int i = 0; i < ARRAY_SIZE(FSK_RogerTable); i++) {
        BK4819_WriteRegister(BK4819_REG_5F, FSK_RogerTable[i]);
    }

    SYSTEM_DelayMs(20);

     
    BK4819_WriteRegister(BK4819_REG_59, 0x0868);

    SYSTEM_DelayMs(180);

     
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);
    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}

void BK4819_PlayRoger(void)
{
    if (gEeprom.ROGER == ROGER_MODE_ROGER) {
        BK4819_PlayRogerNormal();
    } else if (gEeprom.ROGER == ROGER_MODE_MDC) {
        BK4819_PlayRogerMDC();
    }
}

void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
    BK4819_WriteRegister(BK4819_REG_30, 0x0000);
    BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
    *pResult = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

void BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency)
{
    BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

void BK4819_PlayDTMFEx(bool bLocalLoopback, char Code)
{
    BK4819_EnableDTMF();
    BK4819_EnterTxMute();

    BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

    BK4819_WriteRegister(BK4819_REG_70,
        BK4819_REG_70_MASK_ENABLE_TONE1                |
        (DTMF_TONE1_GAIN << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN) |
        BK4819_REG_70_MASK_ENABLE_TONE2                |
        (DTMF_TONE2_GAIN << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));

    BK4819_EnableTXLink();

    SYSTEM_DelayMs(50);

    BK4819_PlayDTMF(Code);

    BK4819_ExitTxMute();
}
