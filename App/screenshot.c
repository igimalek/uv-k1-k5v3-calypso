 

#include "debugging.h"
#include "driver/st7565.h"
#include "screenshot.h"
#include "misc.h"


static uint8_t previousFrame[1024] = {0};
static uint8_t forcedBlock = 0;
static uint8_t keepAlive = 10;

void getScreenShot(bool force)
{
    static uint8_t currentFrame[1024];   
    uint16_t index = 0;
    uint8_t acc = 0;
    uint8_t bitCount = 0;

    if (gUART_LockScreenshot > 0) {
        gUART_LockScreenshot--;
        return;
    }

    if (UART_IsCableConnected()) {
        keepAlive = 10;
    }

    if (keepAlive > 0) {
        if (--keepAlive == 0) return;
    } else {
        return;
    }


    for (uint8_t b = 0; b < 8; b++) {
        for (uint8_t i = 0; i < 128; i++) {
            uint8_t bit = (gStatusLine[i] >> b) & 0x01;
            acc |= (bit << bitCount++);
            if (bitCount == 8) {
                currentFrame[index++] = acc;
                acc = 0;
                bitCount = 0;
            }
        }
    }

     
    for (uint8_t l = 0; l < 7; l++) {
        for (uint8_t b = 0; b < 8; b++) {
            for (uint8_t i = 0; i < 128; i++) {
                uint8_t bit = (gFrameBuffer[l][i] >> b) & 0x01;
                acc |= (bit << bitCount++);
                if (bitCount == 8) {
                    currentFrame[index++] = acc;
                    acc = 0;
                    bitCount = 0;
                }
            }
        }
    }

    if (bitCount > 0)
        currentFrame[index++] = acc;

    if (index != 1024)
        return;  

     
    uint16_t deltaLen = 0;
    uint8_t deltaFrame[128 * 9];   

    for (uint8_t block = 0; block < 128; block++) {
        uint8_t *cur = &currentFrame[block * 8];
        uint8_t *prev = &previousFrame[block * 8];

        bool changed = memcmp(cur, prev, 8) != 0;
        bool isForced = (block == forcedBlock);
        bool fullUpdate = force;

        if (changed || isForced || fullUpdate) {
            deltaFrame[deltaLen++] = block;
            memcpy(&deltaFrame[deltaLen], cur, 8);
            deltaLen += 8;
            memcpy(prev, cur, 8);  
        }
    }

    forcedBlock = (forcedBlock + 1) % 128;

    if (deltaLen == 0)
        return;  

     
    uint8_t header[5] = {
        0xAA, 0x55, 0x02,
        (uint8_t)(deltaLen >> 8),
        (uint8_t)(deltaLen & 0xFF)
    };

    UART_Send(header, 5);
    UART_Send(deltaFrame, deltaLen);
    uint8_t end = 0x0A;
    UART_Send(&end, 1);
}