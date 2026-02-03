 

#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

enum KEY_Code_e {
    KEY_0 = 0,   
    KEY_1,       
    KEY_2,       
    KEY_3,       
    KEY_4,       
    KEY_5,       
    KEY_6,       
    KEY_7,       
    KEY_8,       
    KEY_9,       
    KEY_MENU,    
    KEY_UP,      
    KEY_DOWN,    
    KEY_EXIT,    
    KEY_STAR,    
    KEY_F,       
    KEY_PTT,     
    KEY_SIDE2,   
    KEY_SIDE1,   
    KEY_INVALID  
};
typedef enum KEY_Code_e KEY_Code_t;

extern KEY_Code_t gKeyReading0;
extern KEY_Code_t gKeyReading1;
extern uint16_t   gDebounceCounter;
extern bool       gWasFKeyPressed;

KEY_Code_t KEYBOARD_Poll(void);

#endif

