 

#include <stdint.h>
#include <stdio.h>      

#include "py32f071_ll_bus.h"
#include "py32f071_ll_spi.h"
#include "py32f071_ll_gpio.h"
#include "driver/gpio.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "misc.h"
#include "string.h"

#define SPIx SPI1

#define PIN_CS GPIO_MAKE_PIN(GPIOB, LL_GPIO_PIN_2)
#define PIN_A0 GPIO_MAKE_PIN(GPIOA, LL_GPIO_PIN_6)

uint8_t gStatusLine[LCD_WIDTH];
uint8_t gStatusLineOld[LCD_WIDTH];
uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];
uint8_t gFrameBufferOld[FRAME_LINES][LCD_WIDTH];

static void SPI_Init()
{
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SPI1);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);

    do
    {
        LL_GPIO_InitTypeDef InitStruct;
        LL_GPIO_StructInit(&InitStruct);
        InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
        InitStruct.Alternate = LL_GPIO_AF0_SPI1;
        InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
        InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;

         
        InitStruct.Pin = LL_GPIO_PIN_5;
        InitStruct.Pull = LL_GPIO_PULL_UP;
        LL_GPIO_Init(GPIOA, &InitStruct);

         
        InitStruct.Pin = LL_GPIO_PIN_7;
        InitStruct.Pull = LL_GPIO_PULL_NO;
        LL_GPIO_Init(GPIOA, &InitStruct);
    } while (0);

    LL_SPI_InitTypeDef InitStruct;
    LL_SPI_StructInit(&InitStruct);
    InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    InitStruct.Mode = LL_SPI_MODE_MASTER;
    InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    InitStruct.ClockPolarity = LL_SPI_POLARITY_HIGH;
    InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
    InitStruct.NSS = LL_SPI_NSS_SOFT;
    InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV64;
    LL_SPI_Init(SPIx, &InitStruct);

    LL_SPI_Enable(SPIx);
}

static inline void CS_Assert()
{
    GPIO_ResetOutputPin(PIN_CS);
}

static inline void CS_Release()
{
    GPIO_SetOutputPin(PIN_CS);
}

static inline void A0_Set()
{
    GPIO_SetOutputPin(PIN_A0);
}

static inline void A0_Reset()
{
    GPIO_ResetOutputPin(PIN_A0);
}

static uint8_t SPI_WriteByte(uint8_t Value)
{
    int timeout = 1000000;
    while (!LL_SPI_IsActiveFlag_TXE(SPIx) && timeout--)
        ;

    LL_SPI_TransmitData8(SPIx, Value);

    timeout = 1000000;
    while (!LL_SPI_IsActiveFlag_RXNE(SPIx) && timeout--)
        ;

    return LL_SPI_ReceiveData8(SPIx);
}

static void DrawLine(uint8_t column, uint8_t line, const uint8_t * lineBuffer, unsigned size_defVal)
{   
    ST7565_SelectColumnAndLine(column + 4, line);
    A0_Set();
    for (unsigned i = 0; i < size_defVal; i++) {
        SPI_WriteByte(lineBuffer ? lineBuffer[i] : size_defVal);
    }
}

void ST7565_DrawLine(const unsigned int Column, const unsigned int Line, const uint8_t *pBitmap, const unsigned int Size)
{
    CS_Assert();
    DrawLine(Column, Line, pBitmap, Size);
    CS_Release();
}


#ifdef ENABLE_FEAT_F4HWN


    static void ST7565_BlitScreen(uint8_t line)
    {
        CS_Assert();
        ST7565_WriteByte(0x40);

        if(line == 0)
        {
            DrawLine(0, 0, gStatusLine, LCD_WIDTH);
        }
        else if(line <= FRAME_LINES)
        {
            DrawLine(0, line, gFrameBuffer[line - 1], LCD_WIDTH);
        }
        else
        {
            for (line = 1; line <= FRAME_LINES; line++) {
                DrawLine(0, line, gFrameBuffer[line - 1], LCD_WIDTH);
            }
        }

        CS_Release();
    }

    void ST7565_BlitFullScreen(void)
    {
        if(memcmp(gFrameBuffer, gFrameBufferOld, sizeof(gFrameBuffer)) != 0)
        {
          ST7565_BlitScreen(8);
          memcpy(gFrameBufferOld, gFrameBuffer, sizeof(gFrameBufferOld));
        }
    }

    void ST7565_BlitLine(unsigned line)
    {
        if(memcmp(gFrameBuffer, gFrameBufferOld, sizeof(gFrameBuffer)) != 0)
        {
          ST7565_BlitScreen(line + 1);
          memcpy(gFrameBufferOld, gFrameBuffer, sizeof(gFrameBufferOld));
        }
        
    }

    void ST7565_BlitStatusLine(void)
    {
        if(memcmp(gStatusLine, gStatusLineOld, sizeof(gStatusLine)) != 0)
        {
          ST7565_BlitScreen(0);
          memcpy(gStatusLineOld, gStatusLine, sizeof(gStatusLineOld));
        }
        
    }
#else
    void ST7565_BlitFullScreen(void)
    {
        CS_Assert();
        ST7565_WriteByte(0x40);
        for (unsigned line = 0; line < FRAME_LINES; line++) {
            DrawLine(0, line+1, gFrameBuffer[line], LCD_WIDTH);
        }
        CS_Release();
    }

    void ST7565_BlitLine(unsigned line)
    {
        CS_Assert();
        ST7565_WriteByte(0x40);     
        DrawLine(0, line+1, gFrameBuffer[line], LCD_WIDTH);
        CS_Release();
    }

    void ST7565_BlitStatusLine(void)
    {    
        CS_Assert();
        ST7565_WriteByte(0x40);     
        DrawLine(0, 0, gStatusLine, LCD_WIDTH);
        CS_Release();
    }
#endif

void ST7565_FillScreen(uint8_t value)
{
    CS_Assert();
    for (unsigned i = 0; i < 8; i++) {
         
        DrawLine(0, i, NULL, value);
    }
    CS_Release();
}

 
#define ST7565_CMD_SOFTWARE_RESET 0xE2 


#define ST7565_CMD_BIAS_SELECT 0xA2 


#define ST7565_CMD_COM_DIRECTION 0xC0 


#define ST7565_CMD_SEG_DIRECTION 0xA0 


#define ST7565_CMD_INVERSE_DISPLAY 0xA6 


#define ST7565_CMD_ALL_PIXEL_ON 0xA4 


#define ST7565_CMD_REGULATION_RATIO 0x20 
 
 
#define ST7565_CMD_SET_EV 0x81 


#define ST7565_CMD_POWER_CIRCUIT 0x28 
 
 
#define ST7565_CMD_SET_START_LINE 0x40 


#define ST7565_CMD_DISPLAY_ON_OFF 0xAE 

uint8_t cmds[] = {
    ST7565_CMD_BIAS_SELECT | 0,              
    ST7565_CMD_COM_DIRECTION  | (0 << 3),    
    ST7565_CMD_SEG_DIRECTION | 1,            
    ST7565_CMD_INVERSE_DISPLAY | 0,          
    ST7565_CMD_ALL_PIXEL_ON | 0,             
    ST7565_CMD_REGULATION_RATIO | (4 << 0),  

    ST7565_CMD_SET_EV,                       
    31,

    ST7565_CMD_POWER_CIRCUIT | 0b111,        
    ST7565_CMD_SET_START_LINE | 0,           
    ST7565_CMD_DISPLAY_ON_OFF | 1,           
};

#ifdef ENABLE_FEAT_F4HWN
    static void ST7565_Cmd(uint8_t i)
    {
        switch(i) {
            case 3:
                ST7565_WriteByte(ST7565_CMD_INVERSE_DISPLAY | gSetting_set_inv);
                break;
            case 7:
                ST7565_WriteByte(21 + gSetting_set_ctr);
                break;
            default:
                ST7565_WriteByte(cmds[i]);
        }
    }

    #if defined(ENABLE_FEAT_F4HWN_CTR) || defined(ENABLE_FEAT_F4HWN_INV)
    void ST7565_ContrastAndInv(void)
    {
        CS_Assert();
        ST7565_WriteByte(ST7565_CMD_SOFTWARE_RESET);    

        for(uint8_t i = 0; i < 8; i++)
        {
            ST7565_Cmd(i);
        }

         
    }
    #endif

    int16_t map(int16_t x, int16_t in_min, int16_t in_max, int16_t out_min, int16_t out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

     
    void ST7565_Gauge(uint8_t line, uint8_t min, uint8_t max, uint8_t value)
    {
        gFrameBuffer[line][54] = 0x0c;
        gFrameBuffer[line][55] = 0x12;

        gFrameBuffer[line][121] = 0x12;
        gFrameBuffer[line][122] = 0x0c;

        uint8_t filled = map(value, min, max, 56, 120);

        for (uint8_t i = 56; i <= 120; i++) {
            gFrameBuffer[line][i] = (i <= filled) ? 0x2d : 0x21;
        }
    }
     
#endif
    
void ST7565_Init(void)
{
    SPI_Init();
    ST7565_HardwareReset();
    CS_Assert();
    ST7565_WriteByte(ST7565_CMD_SOFTWARE_RESET);    
    SYSTEM_DelayMs(120);

    for(uint8_t i = 0; i < 8; i++)
    {
#ifdef ENABLE_FEAT_F4HWN
        ST7565_Cmd(i);
#else
        ST7565_WriteByte(cmds[i]);
#endif
    }

    ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b011);    
    SYSTEM_DelayMs(1);
    ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b110);    
    SYSTEM_DelayMs(1);

    for(uint8_t i = 0; i < 4; i++)  
        ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b111);    

    SYSTEM_DelayMs(40);
    
    ST7565_WriteByte(ST7565_CMD_SET_START_LINE | 0);    
    ST7565_WriteByte(ST7565_CMD_DISPLAY_ON_OFF | 1);    

    CS_Release();

    ST7565_FillScreen(0x00);
}

#ifdef ENABLE_FEAT_F4HWN_SLEEP
    void ST7565_ShutDown(void)
    {
        CS_Assert();
        ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b000);    
        ST7565_WriteByte(ST7565_CMD_SET_START_LINE | 0);    
        ST7565_WriteByte(ST7565_CMD_DISPLAY_ON_OFF | 0);    
        CS_Release();
    }
#endif

void ST7565_FixInterfGlitch(void)
{
    CS_Assert();
    for(uint8_t i = 0; i < ARRAY_SIZE(cmds); i++)
#ifdef ENABLE_FEAT_F4HWN
        ST7565_Cmd(i);
#else
        ST7565_WriteByte(cmds[i]);
#endif

    CS_Release();
}

void ST7565_HardwareReset(void)
{
     
     
}

void ST7565_SelectColumnAndLine(uint8_t Column, uint8_t Line)
{
    A0_Reset();
    SPI_WriteByte(Line + 176);
    SPI_WriteByte(((Column >> 4) & 0x0F) | 0x10);
    SPI_WriteByte((Column >> 0) & 0x0F);
}

 
void ST7565_WriteByte(uint8_t Value)
{
    A0_Reset();
    SPI_WriteByte(Value);
}
