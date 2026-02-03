 

#include <string.h>

#include "ui/inputbox.h"

char    gInputBox[8];
char    inputBoxAscii[9];
uint8_t gInputBoxIndex;

void INPUTBOX_Append(const KEY_Code_t Digit)
{
    if (gInputBoxIndex >= sizeof(gInputBox))
        return;

    if (gInputBoxIndex == 0)
        memset(gInputBox, 10, sizeof(gInputBox));

    if (Digit != KEY_INVALID)
        gInputBox[gInputBoxIndex++] = (char)(Digit - KEY_0);
}

const char* INPUTBOX_GetAscii()
{
    for(int i = 0; i < 8; i++) {
        char c = gInputBox[i];
        inputBoxAscii[i] = (c==10)? '-' : '0' + c;
    }
    return inputBoxAscii;
}