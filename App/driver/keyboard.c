 

#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/systick.h"
#include "driver/i2c.h"
#include "misc.h"

KEY_Code_t gKeyReading0     = KEY_INVALID;
KEY_Code_t gKeyReading1     = KEY_INVALID;
uint16_t   gDebounceCounter = 0;

bool       gWasFKeyPressed  = false;

#define GPIOx               GPIOB
#define PIN_MASK_COLS       (LL_GPIO_PIN_6 | LL_GPIO_PIN_5 | LL_GPIO_PIN_4 | LL_GPIO_PIN_3)
#define PIN_COLS            GPIO_MAKE_PIN(GPIOx, PIN_MASK_COLS)
#define PIN_COL(n)          GPIO_MAKE_PIN(GPIOx, 1u << (6 - (n)))

#define PIN_MASK_ROWS       (LL_GPIO_PIN_15 | LL_GPIO_PIN_14 | LL_GPIO_PIN_13 | LL_GPIO_PIN_12)
#define PIN_MASK_ROW(n)     (1u << (15 - (n)))

static inline uint32_t read_rows()
{
    return PIN_MASK_ROWS & LL_GPIO_ReadInputPort(GPIOx);
}

static const KEY_Code_t keyboard[5][4] = {
    {    
         
        KEY_SIDE1, 
        KEY_SIDE2, 

         
        KEY_INVALID, 
        KEY_INVALID, 
    },
    {    
        KEY_MENU, 
        KEY_1, 
        KEY_4, 
        KEY_7, 
    },
    {    
        KEY_UP, 
        KEY_2 , 
        KEY_5 , 
        KEY_8 , 
    },
    {    
        KEY_DOWN, 
        KEY_3   , 
        KEY_6   , 
        KEY_9   , 
    },
    {    
        KEY_EXIT, 
        KEY_STAR, 
        KEY_0   , 
        KEY_F   , 
    }
};

KEY_Code_t KEYBOARD_Poll(void)
{
    KEY_Code_t Key = KEY_INVALID;


    for (unsigned int j = 0; j < 5; j++)
    {
        uint32_t reg;
        unsigned int i;
        unsigned int k;

         
        GPIO_SetOutputPin(PIN_COLS);

         
        if (j > 0)
            GPIO_ResetOutputPin(PIN_COL(j - 1));

         
        for (i = 0, k = 0, reg = 0; i < 3 && k < 8; i++, k++)
        {
            SYSTICK_DelayUs(1);
            uint32_t reg2 = read_rows();
            i *= reg == reg2;
            reg = reg2;
        }

        if (i < 3)
            break;  

        for (unsigned int i = 0; i < 4; i++)
        {
            if (!(reg & PIN_MASK_ROW(i)))
            {
                Key = keyboard[j][i];
                break;
            }
        }

        if (Key != KEY_INVALID)
            break;
    }

    return Key;
}
