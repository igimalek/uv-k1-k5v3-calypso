  

#pragma once
#include "../driver/keyboard.h"

typedef struct {
    KEY_Code_t current;
    KEY_Code_t prev;
    uint8_t    counter;
} KeyboardState;
