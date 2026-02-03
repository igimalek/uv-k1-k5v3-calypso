 

#ifndef UI_INPUTBOX_H
#define UI_INPUTBOX_H

#include <stdint.h>

#include "driver/keyboard.h"

extern char    gInputBox[8];
extern uint8_t gInputBoxIndex;

void INPUTBOX_Append(const KEY_Code_t Digit);
const char* INPUTBOX_GetAscii();

#endif

