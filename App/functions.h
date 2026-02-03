 

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>

enum FUNCTION_Type_t
{
    FUNCTION_FOREGROUND = 0,   
    FUNCTION_TRANSMIT,         
    FUNCTION_MONITOR,          
    FUNCTION_INCOMING,         
    FUNCTION_RECEIVE,          
    FUNCTION_POWER_SAVE,       
    FUNCTION_BAND_SCOPE,       
    FUNCTION_N_ELEM
};

typedef enum FUNCTION_Type_t FUNCTION_Type_t;

extern FUNCTION_Type_t       gCurrentFunction;

void FUNCTION_Init(void);
void FUNCTION_Select(FUNCTION_Type_t Function);
bool FUNCTION_IsRx();

#endif
