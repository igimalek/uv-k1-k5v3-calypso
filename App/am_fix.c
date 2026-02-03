


#include <string.h>

#include "am_fix.h"
#include "app/main.h"
#include "board.h"
#include "driver/bk4819.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#ifdef ENABLE_AGC_SHOW_DATA
#include "ui/main.h"
#endif

#ifdef ENABLE_AM_FIX__

typedef struct
{
    uint16_t reg_val;
    int8_t   gain_dB;
} __attribute__((packed)) t_gain_table;


#define LOOKUP_TABLE 1

#if LOOKUP_TABLE
static const t_gain_table gain_table[] =
{
    {0x03BE, -7},    

    {0x0000,-93},    
    {0x0008,-91},    
    {0x0010,-88},    
    {0x0001,-87},    
    {0x0009,-85},    
    {0x0011,-82},    
    {0x0002,-81},    
    {0x000A,-79},    
    {0x0012,-76},    
    {0x0003,-75},    
    {0x000B,-73},    
    {0x0013,-70},    
    {0x0004,-69},    
    {0x000C,-67},    
    {0x000D,-64},    
    {0x001C,-61},    
    {0x001D,-58},    
    {0x001E,-55},    
    {0x001F,-52},    
    {0x003E,-50},    
    {0x003F,-47},    
    {0x005E,-45},    
    {0x005F,-42},    
    {0x007E,-40},    
    {0x007F,-37},    
    {0x009F,-34},    
    {0x00BF,-32},    
    {0x00DF,-30},    
    {0x00FF,-28},    
    {0x01DF,-26},    
    {0x01FF,-24},    
    {0x02BF,-23},    
    {0x02DF,-21},    
    {0x02FF,-19},    
    {0x035E,-17},    
    {0x035F,-14},    
    {0x037E,-12},    
    {0x037F,-9},     
    {0x038F,-6},     
    {0x03BF,-4},     
    {0x03DF,-2},     
    {0x03FF,0}       
};

const uint8_t gain_table_size = ARRAY_SIZE(gain_table);
#else

t_gain_table gain_table[100] = {{0x03BE, -7}};  
uint8_t gain_table_size = 0;

void CreateTable()
{
typedef union  {
    struct {
        uint8_t pgaIdx:3;
        uint8_t mixerIdx:2;
        uint8_t lnaIdx:3;
        uint8_t lnaSIdx:2;
    };
    uint16_t __raw;
} GainData;

    static const int8_t lna_short_dB[] = {-28, -24, -19,  0};    
    static const int8_t lna_dB[]       = {-24, -19, -14,  -9, -6, -4, -2, 0};
    static const int8_t mixer_dB[]     = { -8,  -6,  -3,   0};
    static const int8_t pga_dB[]       = {-33, -27, -21, -15, -9, -6, -3, 0};

    unsigned i;
    for (uint8_t lnaSIdx = 0; lnaSIdx < ARRAY_SIZE(lna_short_dB); lnaSIdx++) {
        for (uint8_t lnaIdx = 0; lnaIdx < ARRAY_SIZE(lna_dB); lnaIdx++) {
            for (uint8_t mixerIdx = 0; mixerIdx < ARRAY_SIZE(mixer_dB); mixerIdx++) {
                for (uint8_t pgaIdx = 0; pgaIdx < ARRAY_SIZE(pga_dB); pgaIdx++) {
                    int16_t db = lna_short_dB[lnaSIdx] + lna_dB[lnaIdx] + mixer_dB[mixerIdx] + pga_dB[pgaIdx];
                    GainData gainData = {{
                        pgaIdx,
                        mixerIdx,
                        lnaIdx,
                        lnaSIdx,
                    }};

                    for (i = 1; i < ARRAY_SIZE(gain_table); i++) {
                        t_gain_table * gain = &gain_table[i];
                        if (db == gain->gain_dB)
                            break;
                        if (db > gain->gain_dB)
                            continue;
                        if (db < gain->gain_dB) {
                            if(gain->gain_dB)
                                memmove(gain + 1, gain, 100 - i);
                            gain->gain_dB = db;
                            gain->reg_val = gainData.__raw;
                            break;
                        }
                        gain->gain_dB = db;
                        gain->reg_val = gainData.__raw;
                        break;
                    }
                }
            }
        }
    }

    gain_table_size = i+1;
}
#endif


#ifdef ENABLE_AM_FIX___SHOW_DATA
     
    static const unsigned int display_update_rate = 250 / 10;    
    unsigned int counter = 0;
#endif

unsigned int gain_table_index[2] = {0, 0};
 
unsigned int gain_table_index_prev[2] = {0, 0};
 
int16_t prev_rssi[2] = {0, 0};
 
unsigned int hold_counter[2] = {0, 0};
 
const int16_t desired_rssi = (-89 + 160) * 2;

int8_t currentGainDiff;
bool enabled = true;

void AM_fix_init(void)
{    
    for (int i = 0; i < 2; i++) {
        gain_table_index[i] = 0;   
    }
#if !LOOKUP_TABLE
    CreateTable();
#endif
}

void AM_fix_reset(const unsigned vfo)
{    
    if (vfo > 1)
        return;

    #ifdef ENABLE_AM_FIX___SHOW_DATA
        counter = 0;
    #endif

    prev_rssi[vfo] = 0;
    hold_counter[vfo] = 0;
    gain_table_index_prev[vfo] = 0;
}


void AM_fix_10ms(const unsigned vfo)
{
    if(!gSetting_AM_fix || !enabled || vfo > 1 )
        return;

    if (gCurrentFunction != FUNCTION_FOREGROUND && !FUNCTION_IsRx()) {
#ifdef ENABLE_AM_FIX___SHOW_DATA
        counter = display_update_rate;   
#endif
        return;
    }

#ifdef ENABLE_AM_FIX___SHOW_DATA
    if (counter > 0) {
        if (++counter >= display_update_rate) {  
            counter        = 0;
            gUpdateDisplay = true;
        }
    }
#endif

    static uint32_t lastFreq[2];
    if(gEeprom.VfoInfo[vfo].pRX->Frequency != lastFreq[vfo]) {
        lastFreq[vfo] = gEeprom.VfoInfo[vfo].pRX->Frequency;
        AM_fix_reset(vfo);
    }

    int16_t rssi;
    {    
         
        const int16_t new_rssi = BK4819_GetRSSI();
        rssi                   = (prev_rssi[vfo] > 0) ? (prev_rssi[vfo] + new_rssi) / 2 : new_rssi;
        prev_rssi[vfo]         = new_rssi;
    }

#ifdef ENABLE_AM_FIX___SHOW_DATA
    {
        static int16_t lastRssi;

        if (lastRssi != rssi) {  
            lastRssi = rssi;

            if (counter == 0) {
                counter        = 1;
                gUpdateDisplay = true;  
            }
        }
    }
#endif


    if (hold_counter[vfo] > 0)
        hold_counter[vfo]--;

     
    int16_t diff_dB = (rssi - desired_rssi) / 2;

    if (diff_dB > 0) {   
        unsigned int index = gain_table_index[vfo];    

        if (diff_dB >= 10) {     
             

            const int16_t desired_gain_dB = (int16_t)gain_table[index].gain_dB - diff_dB + 8;  

             
            while (index > 1)
                if (gain_table[--index].gain_dB <= desired_gain_dB)
                    break;
        }
        else
        {    
            if (index > 1)
                index--;      
        }

        index = MAX(1u, index);

        if (gain_table_index[vfo] != index)
        {
            gain_table_index[vfo] = index;
            hold_counter[vfo] = 30;        
        }
    }

    if (diff_dB >= -6)                     
        hold_counter[vfo] = 30;            

    if (hold_counter[vfo] == 0)
    {    
        const unsigned int index = gain_table_index[vfo] + 1;                  
        gain_table_index[vfo] = MIN(index, gain_table_size - 1u);
    }


    {    
        const unsigned int index = gain_table_index[vfo];

         
        gain_table_index_prev[vfo] = index;
        currentGainDiff = gain_table[0].gain_dB - gain_table[index].gain_dB;
        BK4819_WriteRegister(BK4819_REG_13, gain_table[index].reg_val);
#ifdef ENABLE_AGC_SHOW_DATA
        UI_MAIN_PrintAGC(true);
#endif
    }

#ifdef ENABLE_AM_FIX___SHOW_DATA
    if (counter == 0) {
        counter        = 1;
        gUpdateDisplay = true;
    }
#endif
}

#ifdef ENABLE_AM_FIX___SHOW_DATA
void AM_fix_print_data(const unsigned vfo, char *s) {
    if (s != NULL && vfo < ARRAY_SIZE(gain_table_index)) {
        const unsigned int index = gain_table_index[vfo];
        sprintf(s, "%2u %4ddB %3u", index, gain_table[index].gain_dB, prev_rssi[vfo]);
        counter = 0;
    }
}
#endif

int8_t AM_fix_get_gain_diff()
{
    return currentGainDiff;
}

void AM_fix_enable(bool on)
{
    enabled = on;
}
#endif
