 

#ifndef DRIVER_ST7565_H
#define DRIVER_ST7565_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_WIDTH       128
#define LCD_HEIGHT       64
#define FRAME_LINES 7

extern uint8_t gStatusLine[LCD_WIDTH];
extern uint8_t gStatusLineOld[LCD_WIDTH];
extern uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];
extern uint8_t gFrameBufferOld[FRAME_LINES][LCD_WIDTH];

void ST7565_DrawLine(const unsigned int Column, const unsigned int Line, const uint8_t *pBitmap, const unsigned int Size);
void ST7565_BlitFullScreen(void);
void ST7565_BlitLine(unsigned line);
void ST7565_BlitStatusLine(void);
void ST7565_FillScreen(uint8_t Value);
void ST7565_Init(void);
#ifdef ENABLE_FEAT_F4HWN_SLEEP
    void ST7565_ShutDown(void);
#endif
void ST7565_FixInterfGlitch(void);
void ST7565_HardwareReset(void);
void ST7565_SelectColumnAndLine(uint8_t Column, uint8_t Line);
void ST7565_WriteByte(uint8_t Value);

#ifdef ENABLE_FEAT_F4HWN
    #if defined(ENABLE_FEAT_F4HWN_CTR) || defined(ENABLE_FEAT_F4HWN_INV)
    void ST7565_ContrastAndInv(void);
    #endif
     
    void ST7565_Gauge(uint8_t line, uint8_t min, uint8_t max, uint8_t value);
     
    int16_t map(int16_t x, int16_t in_min, int16_t in_max, int16_t out_min, int16_t out_max);
#endif

#endif

