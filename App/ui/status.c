/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
#ifndef ENABLE_FEAT_F4HWN_DEBUG
static void convertTime(uint8_t *line, uint8_t type) 
{
    uint16_t t = (type == 0) ? (gTxTimerCountdown_500ms / 2) : (3600 - gRxTimerCountdown_500ms / 2);
    uint8_t m = t / 60;
    uint8_t s = t - (m * 60);

    char str[6];
    sprintf(str, "%02u:%02u", m, s);
    UI_PrintStringSmallBufferNormal(str, line);

    gUpdateStatus = true;
}
#endif
#endif

void UI_DisplayStatus()
{
    char str[12] = "";
    gUpdateStatus = false;
    memset(gStatusLine, 0, sizeof(gStatusLine));

    // ТВОИ КООРДИНАТЫ (X)
    const uint8_t POS_TMR  = 10;   // Таймер (текст)
    const uint8_t POS_MOD  = 0;   // DW, XB, MO (глифы)
    const uint8_t POS_VOX  = 59;   // VOX (глиф)
   // PTTDEL const uint8_t POS_PTT  = 70;   // PTT (глифы)
    const uint8_t POS_B    = 80;   // Подсветка (глиф)
    const uint8_t POS_LOCK = 90;   // Замок (глиф)
    const uint8_t POS_F    = 90;   // Буква F (глиф)

   

    // 2. РЕЖИМЫ (DW, DWR, HL, XB, MO) - В СТОЛБИК
    if (!SCANNER_IsScanning()) {
        uint8_t dw = (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF) + (gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF) * 2;
        if (dw == 1 || dw == 3) {
            if (gDualWatchActive) {
                if (dw == 1) { // DW (tx)
                    gStatusLine[POS_MOD + 0] |= 0x7F;
                    gStatusLine[POS_MOD + 1] |= 0x6B;
                    gStatusLine[POS_MOD + 2] |= 0x49;
                    gStatusLine[POS_MOD + 3] |= 0x08;
                    gStatusLine[POS_MOD + 4] |= 0x49;
                    gStatusLine[POS_MOD + 5] |= 0x6B;
                    gStatusLine[POS_MOD + 6] |= 0x7F;

                } else { // DWR
                    gStatusLine[POS_MOD + 0] |= 0x7F;
                    gStatusLine[POS_MOD + 1] |= 0x6B;
                    gStatusLine[POS_MOD + 2] |= 0x4D;
                    gStatusLine[POS_MOD + 3] |= 0x0E;
                    gStatusLine[POS_MOD + 4] |= 0x4D;
                    gStatusLine[POS_MOD + 5] |= 0x6B;
                    gStatusLine[POS_MOD + 6] |= 0x7F;

                }
            } else { // HL (HOLD)
                    gStatusLine[POS_MOD + 0] |= 0x7F;
                    gStatusLine[POS_MOD + 1] |= 0x49;
                    gStatusLine[POS_MOD + 2] |= 0x41;
                    gStatusLine[POS_MOD + 3] |= 0x63;
                    gStatusLine[POS_MOD + 4] |= 0x41;
                    gStatusLine[POS_MOD + 5] |= 0x49;
                    gStatusLine[POS_MOD + 6] |= 0x7F;
            }
        } else if (dw == 2) { // XB
                gStatusLine[POS_MOD + 0] |= 0x7F;
                gStatusLine[POS_MOD + 1] |= 0x71;
                gStatusLine[POS_MOD + 2] |= 0x71;
                gStatusLine[POS_MOD + 3] |= 0x7F;
                gStatusLine[POS_MOD + 4] |= 0x47;
                gStatusLine[POS_MOD + 5] |= 0x47;
                gStatusLine[POS_MOD + 6] |= 0x7F;
        } else { // MO
                gStatusLine[POS_MOD + 0] |= 0x7F;
                gStatusLine[POS_MOD + 1] |= 0x51;
                gStatusLine[POS_MOD + 2] |= 0x71;
                gStatusLine[POS_MOD + 3] |= 0x51;
                gStatusLine[POS_MOD + 4] |= 0x71;
                gStatusLine[POS_MOD + 5] |= 0x51;
                gStatusLine[POS_MOD + 6] |= 0x7F;

        }
    }

    // 3. ТАЙМЕР
#ifdef ENABLE_FEAT_F4HWN_RX_TX_TIMER
    if (gSetting_set_tmr) {
        if (gCurrentFunction == FUNCTION_TRANSMIT) convertTime(gStatusLine + POS_TMR, 0);
        else if (FUNCTION_IsRx()) convertTime(gStatusLine + POS_TMR, 1);
    }
#endif

    // 4. VOX - В СТОЛБИК
#ifdef ENABLE_VOX
    if (gEeprom.VOX_SWITCH) {
        gStatusLine[POS_VOX + 0] |= 0x7F;
        gStatusLine[POS_VOX + 1] |= 0x41;
        gStatusLine[POS_VOX + 2] |= 0x1C;
        gStatusLine[POS_VOX + 3] |= 0x7C;
        gStatusLine[POS_VOX + 4] |= 0x1C;
        gStatusLine[POS_VOX + 5] |= 0x41;
        gStatusLine[POS_VOX + 6] |= 0x7F;
    }
#endif

//     // 5. PTT - В СТОЛБИК
// #ifdef ENABLE_FEAT_F4HWN
//     if (gSetting_set_ptt_session) {
//         gStatusLine[POS_PTT + 0] |= 0x3E;
//         gStatusLine[POS_PTT + 1] |= 0x7F;
//         gStatusLine[POS_PTT + 2] |= 0x5D;
//         gStatusLine[POS_PTT + 3] |= 0x49;
//         gStatusLine[POS_PTT + 4] |= 0x41;
//         gStatusLine[POS_PTT + 5] |= 0x41;
//         gStatusLine[POS_PTT + 6] |= 0x3E;
//     } else {
//         gStatusLine[POS_PTT + 0] |= 0x3E;
//         gStatusLine[POS_PTT + 1] |= 0x41;
//         gStatusLine[POS_PTT + 2] |= 0x63;
//         gStatusLine[POS_PTT + 3] |= 0x77;
//         gStatusLine[POS_PTT + 4] |= 0x7F;
//         gStatusLine[POS_PTT + 5] |= 0x7F;
//         gStatusLine[POS_PTT + 6] |= 0x3E;
//     }
// #endif

    // 6. ПОДСВЕТКА (B) - В СТОЛБИК
    if (gBackLight) {
        gStatusLine[POS_B + 0] |= 0x0C;
        gStatusLine[POS_B + 1] |= 0x12;
        gStatusLine[POS_B + 2] |= 0x65;
        gStatusLine[POS_B + 3] |= 0x79;
        gStatusLine[POS_B + 4] |= 0x65;
        gStatusLine[POS_B + 5] |= 0x12;
        gStatusLine[POS_B + 6] |= 0x0C;
    }

    // 7. F-KEY И ЗАМОК - РАЗДЕЛЬНО
    if (gWasFKeyPressed) {
        gStatusLine[POS_F + 0] |= 0x7F;
        gStatusLine[POS_F + 1] |= 0x41;
        gStatusLine[POS_F + 2] |= 0x75;
        gStatusLine[POS_F + 3] |= 0x75;
        gStatusLine[POS_F + 4] |= 0x75;
        gStatusLine[POS_F + 5] |= 0x7D;
        gStatusLine[POS_F + 6] |= 0x7F;
    }
    if (gEeprom.KEY_LOCK) {
        gStatusLine[POS_LOCK + 0] |= 0x7C;
        gStatusLine[POS_LOCK + 1] |= 0x7A;
        gStatusLine[POS_LOCK + 2] |= 0x79;
        gStatusLine[POS_LOCK + 3] |= 0x49;
        gStatusLine[POS_LOCK + 4] |= 0x79;
        gStatusLine[POS_LOCK + 5] |= 0x7A;
        gStatusLine[POS_LOCK + 6] |= 0x7C;
    }

     // 1. СКАНЕР И СПИСКИ
    if (gScanStateDir != SCAN_OFF || SCANNER_IsScanning()) {
        if (IS_MR_CHANNEL(gNextMrChannel) && !SCANNER_IsScanning()) {
            switch(gEeprom.SCAN_LIST_DEFAULT) {
                case 0: memcpy(gStatusLine + 0, BITMAP_ScanList0, sizeof(BITMAP_ScanList0)); break;
                case 1: memcpy(gStatusLine + 0, BITMAP_ScanList1, sizeof(BITMAP_ScanList1)); break;
                case 2: memcpy(gStatusLine + 0, BITMAP_ScanList2, sizeof(BITMAP_ScanList2)); break;
                case 3: memcpy(gStatusLine + 0, BITMAP_ScanList3, sizeof(BITMAP_ScanList3)); break;
                case 4: memcpy(gStatusLine + 0, BITMAP_ScanList123, sizeof(BITMAP_ScanList123)); break;
                case 5: memcpy(gStatusLine + 0, BITMAP_ScanListAll, sizeof(BITMAP_ScanListAll)); break;
            }
        } else {
            memcpy(gStatusLine + POS_MOD + 1, gFontS, sizeof(gFontS));
        }
    }


    #ifdef ENABLE_FEAT_F4HWN
    if (gMute) {
        // Вывод Mute (например на позиции 100, чтоб не мешал)
        memcpy(gStatusLine + 100, gFontMute, sizeof(gFontMute));
    }
    #endif

    // 8. БАТАРЕЯ (Твой оригинал)
    if (gSetting_battery_text == 1) {
        sprintf(str, "%u.%02u", gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100);
    } else {
        sprintf(str, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
    }
    
    uint8_t battPos = 127 - (strlen(str) * 7);
    UI_PrintStringSmallBufferBold(str, gStatusLine + battPos);

    ST7565_BlitStatusLine();
}