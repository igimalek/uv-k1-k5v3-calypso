  

#pragma once

#include "keyboard_state.h"

#include "../bitmaps.h"
#include "../board.h"
#include "py32f0xx.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include "../audio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BRICK_NUMBER 18
#define BALL_NUMBER  5

typedef struct {
    uint8_t x;         
    uint8_t y;         
    uint8_t w;         
    uint8_t h;         
    uint8_t s;         
    bool destroy;      
} Brick;

typedef struct {
    int8_t x;     
    uint8_t y;    
    uint8_t w;    
    uint8_t h;    
    uint8_t p;    
} Racket;

typedef struct {
    int16_t x;    
    int8_t y;     
    uint8_t w;    
    uint8_t h;    
    int8_t dx;    
    int8_t dy;    
} Ball;

void initWall(void);
void drawWall(void);
void initRacket(void);
void drawRacket(void);
void APP_RunBreakout(void);
