 

#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stdint.h>

enum GUI_DisplayType_t
{
    DISPLAY_MAIN = 0,               
    DISPLAY_MENU,                   
    DISPLAY_SCANNER,                

#ifdef ENABLE_FMRADIO
    DISPLAY_FM,                     
#endif

#ifdef ENABLE_AIRCOPY
    DISPLAY_AIRCOPY,                
#endif

    DISPLAY_N_ELEM,                 
    DISPLAY_INVALID = 0xFFu         
};

typedef enum GUI_DisplayType_t GUI_DisplayType_t;

extern GUI_DisplayType_t gScreenToDisplay;

extern GUI_DisplayType_t gRequestDisplayScreen;

extern uint8_t           gAskForConfirmation;

extern bool              gAskToSave;

extern bool              gAskToDelete;

void GUI_DisplayScreen(void);

void GUI_SelectNextDisplay(GUI_DisplayType_t Display);

#endif
