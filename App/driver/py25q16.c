/* Copyright 2025 muzkr
 * https://github.com/muzkr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#include "driver/py25q16.h"
#include "driver/gpio.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_system.h"
#include "py32f071_ll_spi.h"
#include "py32f071_ll_dma.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "external/printf/printf.h"

// #define DEBUG

#define SPIx SPI2
#define CHANNEL_RD LL_DMA_CHANNEL_4
#define CHANNEL_WR LL_DMA_CHANNEL_5

#define CS_PIN GPIO_MAKE_PIN(GPIOA, LL_GPIO_PIN_3)

#define SECTOR_SIZE 0x1000
#define PAGE_SIZE 0x100

static uint32_t SectorCacheAddr = 0x1000000;
static uint8_t SectorCache[SECTOR_SIZE];
static uint8_t BlackHole[1];
static volatile bool TC_Flag;

static inline void CS_Assert()
{
    GPIO_ResetOutputPin(CS_PIN);
}

static inline void CS_Release()
{
    GPIO_SetOutputPin(CS_PIN);
}

// py25q16 timing parameters from datasheet
/*
Symbol Parameter  2.3V~3.6V Units Min Typ Max 
TESL(4) Erase Suspend Latency   30 us 
TPSL(4) Program Suspend Latency   30 us 
TPRS(2) Latency between Program Resume and next Suspend 0.5   us 
TERS(3) Latency between Erase Resume and next Suspend 0.5   us 
tPSR Program Security Registers time (up to 256 bytes)  0.4 2.4 ms 
tESR Erase Security Registers time  40 300 ms 
tPP Page program time (up to 256 bytes)  0.4 2.4 ms 
tbp Byte program time(1byte)  30 50 us 
tSE Sector erase time  40 300 ms 
tBE1 Block erase time for 32K bytes  0.12 0.8 s 
tBE2 Block erase time for 64K bytes  0.15 1.2 s 
tCE Chip erase time   5 15 s
*/

// ============================================================================
// SPI_Init - Initialize SPI peripheral and DMA for flash communication
// ============================================================================
// Configures SPI2 for master mode with DMA support for efficient data transfer.
// Sets up GPIO pins for SPI signals and enables DMA channels for read/write.
//
// Dependencies:
//   - LL_APB1_GRP1_EnableClock() - enable SPI2 clock
//   - LL_AHB1_GRP1_EnableClock() - enable DMA1 clock
//   - LL_IOP_GRP1_EnableClock() - enable GPIOA clock
//   - LL_GPIO_Init() - configure GPIO pins
//   - LL_SYSCFG_SetDMARemap() - map DMA channels to SPI
//   - NVIC_SetPriority(), NVIC_EnableIRQ() - enable DMA interrupts
//   - LL_SPI_Init() - configure SPI parameters
//
// Timing:
//   - Initialization: ~1ms (clock enables and register setup)
//
// Notes:
//   - SPI mode: Full-duplex, CPOL=1, CPHA=1 (mode 3)
//   - Baud rate: Prescaler DIV2 (high speed for flash)
//   - DMA channels: CH4 for RX, CH5 for TX
//   - Interrupts: DMA transfer complete on CH4
//
static void SPI_Init()
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);

    do
    {
        // SCK: PA0
        // MOSI: PA1
        // MISO: PA2

        LL_GPIO_InitTypeDef InitStruct;
        LL_GPIO_StructInit(&InitStruct);
        InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
        InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
        InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
        InitStruct.Pull = LL_GPIO_PULL_UP;

        InitStruct.Pin = LL_GPIO_PIN_0;
        InitStruct.Alternate = LL_GPIO_AF8_SPI2;
        LL_GPIO_Init(GPIOA, &InitStruct);

        InitStruct.Pin = LL_GPIO_PIN_1 | LL_GPIO_PIN_2;
        InitStruct.Alternate = LL_GPIO_AF9_SPI2;
        LL_GPIO_Init(GPIOA, &InitStruct);

    } while (0);

    LL_SYSCFG_SetDMARemap(DMA1, CHANNEL_RD, LL_SYSCFG_DMA_MAP_SPI2_RD);
    LL_SYSCFG_SetDMARemap(DMA1, CHANNEL_WR, LL_SYSCFG_DMA_MAP_SPI2_WR);

    NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Channel4_5_6_7_IRQn);

    LL_SPI_InitTypeDef InitStruct;
    LL_SPI_StructInit(&InitStruct);
    InitStruct.Mode = LL_SPI_MODE_MASTER;
    InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
    InitStruct.ClockPolarity = LL_SPI_POLARITY_HIGH;
    InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2;  // calypso marker slow down SPI for stability
    InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    InitStruct.NSS = LL_SPI_NSS_SOFT;
    InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    LL_SPI_Init(SPIx, &InitStruct);

    LL_SPI_Enable(SPIx);
}

// ============================================================================
// SPI_ReadBuf - Read data from SPI using DMA (bulk transfer)
// ============================================================================
// Performs efficient bulk read from SPI peripheral using DMA.
// Configures DMA channels for peripheral-to-memory transfer.
// Uses dummy writes to BlackHole buffer to clock out data.
//
// Parameters:
//   Buf:  Destination buffer for received data
//   Size: Number of bytes to read
//
// Dependencies:
//   - DMA1 channels 4 (RX) and 5 (TX)
//   - SPI2 peripheral
//   - TC_Flag (volatile bool) - set by DMA ISR on completion
//   - BlackHole[1] - dummy buffer for TX
//
// Timing:
//   - Setup: ~10-20us (DMA config and SPI enable)
//   - Transfer: Size bytes @ SPI baud rate (~1us per byte)
//   - Wait: Blocking until DMA complete (TC_Flag set in ISR)
//   - Total: ~Size us + overhead (blocking)
//
// Notes:
//   - TC_Flag is set to false before transfer, polled in while loop
//   - DMA ISR (DMA1_Channel4_5_6_7_IRQHandler) sets TC_Flag = true on completion
//   - While (!TC_Flag) loop waits for transfer to finish
//   - Full-duplex: RX from SPI, TX dummy data
//
static void SPI_ReadBuf(uint8_t *Buf, uint32_t Size)
{
    LL_SPI_Disable(SPIx);
    LL_DMA_DisableChannel(DMA1, CHANNEL_RD);
    LL_DMA_DisableChannel(DMA1, CHANNEL_WR);

    LL_DMA_ClearFlag_GI4(DMA1);
    LL_DMA_ClearFlag_GI5(DMA1); // calypso marker

    LL_DMA_ConfigTransfer(DMA1, CHANNEL_RD,                 //
                          LL_DMA_DIRECTION_PERIPH_TO_MEMORY //
                              | LL_DMA_MODE_NORMAL          //
                              | LL_DMA_PERIPH_NOINCREMENT   //
                              | LL_DMA_MEMORY_INCREMENT     //
                              | LL_DMA_PDATAALIGN_BYTE      //
                              | LL_DMA_MDATAALIGN_BYTE      //
                              | LL_DMA_PRIORITY_MEDIUM      //
    );

    LL_DMA_ConfigTransfer(DMA1, CHANNEL_WR,                 //
                          LL_DMA_DIRECTION_MEMORY_TO_PERIPH //
                              | LL_DMA_MODE_NORMAL          //
                              | LL_DMA_PERIPH_NOINCREMENT   //
                              | LL_DMA_MEMORY_NOINCREMENT   //
                              | LL_DMA_PDATAALIGN_BYTE      //
                              | LL_DMA_MDATAALIGN_BYTE      //
                              | LL_DMA_PRIORITY_MEDIUM      //
    );

    LL_DMA_SetMemoryAddress(DMA1, CHANNEL_RD, (uint32_t)Buf);
    LL_DMA_SetPeriphAddress(DMA1, CHANNEL_RD, LL_SPI_DMA_GetRegAddr(SPIx));
    LL_DMA_SetDataLength(DMA1, CHANNEL_RD, Size);

    LL_DMA_SetMemoryAddress(DMA1, CHANNEL_WR, (uint32_t)BlackHole);
    LL_DMA_SetPeriphAddress(DMA1, CHANNEL_WR, LL_SPI_DMA_GetRegAddr(SPIx));
    LL_DMA_SetDataLength(DMA1, CHANNEL_WR, Size);

    TC_Flag = false;
    LL_DMA_EnableIT_TC(DMA1, CHANNEL_RD);
    LL_DMA_EnableChannel(DMA1, CHANNEL_RD);
    LL_DMA_EnableChannel(DMA1, CHANNEL_WR);

    LL_SPI_EnableDMAReq_RX(SPIx);
    LL_SPI_Enable(SPIx);
    LL_SPI_EnableDMAReq_TX(SPIx);
    
    // calypso marker
    // Add safety timeout to prevent infinite hang
    uint32_t timeout = 1000000; // ~1 second safety timeout
    while (!TC_Flag && timeout--)
        ;

    // calypso marker
    if (!TC_Flag) {
        // Timeout occurred - disable everything to prevent further issues
        LL_SPI_Disable(SPIx);
        LL_DMA_DisableChannel(DMA1, CHANNEL_RD);
        LL_DMA_DisableChannel(DMA1, CHANNEL_WR);
#ifdef DEBUG
        printf("ERROR: SPI_ReadBuf timeout - DMA did not complete\n");
#endif
    }
}

// ============================================================================
// SPI_WriteBuf - Write data to SPI using DMA (bulk transfer)
// ============================================================================
// Performs efficient bulk write to SPI peripheral using DMA.
// Configures DMA channels for memory-to-peripheral transfer.
// Receives dummy data into BlackHole buffer during transmission.
//
// Parameters:
//   Buf:  Source buffer containing data to send
//   Size: Number of bytes to write
//
// Dependencies:
//   - DMA1 channels 4 (RX) and 5 (TX)
//   - SPI2 peripheral
//   - TC_Flag (volatile bool) - set by DMA ISR on completion
//   - BlackHole[1] - dummy buffer for RX
//
// Timing:
//   - Setup: ~10-20us (DMA config and SPI enable)
//   - Transfer: Size bytes @ SPI baud rate (~1us per byte)
//   - Wait: Blocking until DMA complete (TC_Flag set in ISR)
//   - Total: ~Size us + overhead (blocking)
//
// Notes:
//   - TC_Flag is set to false before transfer, polled in while loop
//   - DMA ISR (DMA1_Channel4_5_6_7_IRQHandler) sets TC_Flag = true on completion
//   - While (!TC_Flag) loop waits for transfer to finish
//   - Full-duplex: TX to SPI, RX dummy data
//
static void SPI_WriteBuf(const uint8_t *Buf, uint32_t Size)
{
    LL_SPI_Disable(SPIx);
    LL_DMA_DisableChannel(DMA1, CHANNEL_RD);
    LL_DMA_DisableChannel(DMA1, CHANNEL_WR);

    LL_DMA_ClearFlag_GI4(DMA1);
    LL_DMA_ClearFlag_GI5(DMA1); // calypso marker

    LL_DMA_ConfigTransfer(DMA1, CHANNEL_RD,                 //
                          LL_DMA_DIRECTION_PERIPH_TO_MEMORY //
                              | LL_DMA_MODE_NORMAL          //
                              | LL_DMA_PERIPH_NOINCREMENT   //
                              | LL_DMA_MEMORY_NOINCREMENT   //
                              | LL_DMA_PDATAALIGN_BYTE      //
                              | LL_DMA_MDATAALIGN_BYTE      //
                              | LL_DMA_PRIORITY_LOW         //
    );

    LL_DMA_ConfigTransfer(DMA1, CHANNEL_WR,                 //
                          LL_DMA_DIRECTION_MEMORY_TO_PERIPH //
                              | LL_DMA_MODE_NORMAL          //
                              | LL_DMA_PERIPH_NOINCREMENT   //
                              | LL_DMA_MEMORY_INCREMENT     //
                              | LL_DMA_PDATAALIGN_BYTE      //
                              | LL_DMA_MDATAALIGN_BYTE      //
                              | LL_DMA_PRIORITY_LOW         //
    );

    LL_DMA_SetMemoryAddress(DMA1, CHANNEL_RD, (uint32_t)BlackHole);
    LL_DMA_SetPeriphAddress(DMA1, CHANNEL_RD, LL_SPI_DMA_GetRegAddr(SPIx));
    LL_DMA_SetDataLength(DMA1, CHANNEL_RD, Size);

    LL_DMA_SetMemoryAddress(DMA1, CHANNEL_WR, (uint32_t)Buf);
    LL_DMA_SetPeriphAddress(DMA1, CHANNEL_WR, LL_SPI_DMA_GetRegAddr(SPIx));
    LL_DMA_SetDataLength(DMA1, CHANNEL_WR, Size);

    TC_Flag = false;
    LL_DMA_EnableIT_TC(DMA1, CHANNEL_RD);
    LL_DMA_EnableChannel(DMA1, CHANNEL_RD);
    LL_DMA_EnableChannel(DMA1, CHANNEL_WR);

    LL_SPI_EnableDMAReq_RX(SPIx);
    LL_SPI_Enable(SPIx);
    LL_SPI_EnableDMAReq_TX(SPIx);

    // calypso marker
    // Add safety timeout to prevent infinite hang
    uint32_t timeout = 1000000; // ~1 second safety timeout
    while (!TC_Flag && timeout--)
        ;
    
    // calypso marker    
    if (!TC_Flag) {
        // Timeout occurred - disable everything to prevent further issues
        LL_SPI_Disable(SPIx);
        LL_DMA_DisableChannel(DMA1, CHANNEL_RD);
        LL_DMA_DisableChannel(DMA1, CHANNEL_WR);
#ifdef DEBUG
        printf("ERROR: SPI_WriteBuf timeout - DMA did not complete\n");
#endif
    }
}

// ============================================================================
// SPI_WriteByte - Transmit and receive a single byte via SPI (polling)
// ============================================================================
// Sends one byte to SPI and receives one byte in return.
// Uses polling for TXE (transmit buffer empty) and RXNE (receive buffer not empty).
//
// Parameters:
//   Value: Byte to transmit
//
// Returns:
//   Received byte from SPI
//
// Dependencies:
//   - SPI2 peripheral
//   - LL_SPI_IsActiveFlag_TXE() - check transmit ready
//   - LL_SPI_IsActiveFlag_RXNE() - check receive ready
//   - LL_SPI_TransmitData8(), LL_SPI_ReceiveData8() - data transfer
//
// Timing:
//   - Wait TXE: ~1-5us (depends on SPI speed)
//   - Transmit: immediate
//   - Wait RXNE: ~1-5us
//   - Receive: immediate
//   - Total: ~2-10us (blocking)
//
// Notes:
//   - Blocking function; waits for SPI hardware
//   - Used for small transfers or when DMA overhead not justified
//
static uint8_t SPI_WriteByte(uint8_t Value)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPIx))
        ;
    LL_SPI_TransmitData8(SPIx, Value);
    while (!LL_SPI_IsActiveFlag_RXNE(SPIx))
        ;
    return LL_SPI_ReceiveData8(SPIx);
}

static void WriteAddr(uint32_t Addr);
static uint8_t ReadStatusReg(uint32_t Which);
static void WaitWIP();
static void WriteEnable();
static void SectorErase(uint32_t Addr);
static void SectorProgram(uint32_t Addr, const uint8_t *Buf, uint32_t Size);
static void PageProgram(uint32_t Addr, const uint8_t *Buf, uint32_t Size);

void PY25Q16_Init()
{
    CS_Release();
    SPI_Init();
}

void PY25Q16_ReadBuffer(uint32_t Address, void *pBuffer, uint32_t Size)
{
#ifdef DEBUG
    printf("spi flash read: %06x %ld\n", Address, Size);
#endif
    CS_Assert();

    SPI_WriteByte(0x03); // Fast read
    WriteAddr(Address);

    if (Size >= 16)
    {
        SPI_ReadBuf((uint8_t *)pBuffer, Size);
    }
    else
    {
        for (uint32_t i = 0; i < Size; i++)
        {
            ((uint8_t *)(pBuffer))[i] = SPI_WriteByte(0xff);
        }
    }

    CS_Release();
    
}

// ============================================================================
// PY25Q16_WriteBuffer - Flash write with sector caching and wear leveling
// ============================================================================
// Writes data to flash with intelligent sector caching to minimize erase cycles.
// Compares new data with cached sector; only erases if data differs from 0xFF.
// Optimized for write operations: avoids unnecessary erases using memcmp().
//
// Parameters:
//   Address: 24-bit flash address (0x000000 - 0x1FFFFF)
//   pBuffer: Pointer to source data
//   Size:    Number of bytes to write (any size; handles sector boundaries)
//   Append:  true=partial sector updates; false=full sector write
//
// Dependencies:
//   - PY25Q16_ReadBuffer() - reads sector into cache
//   - SectorErase() - erases 4KB sectors
//   - SectorProgram() - programs up to 4KB
//   - memcmp(), memcpy(), memset() - data manipulation
//   - Static cache: SectorCache[4096], SectorCacheAddr
//
// Timing:
//   - No change: ~1ms (cache hit, memcmp detects no change)
//   - Program only: ~5-50ms (partial write, no erase)
//   - Erase + program: ~50-150ms (must erase sector)
//   - Multiple sectors: Add ~50-150ms per sector
//   - Blocking; CPU waits for flash completion
//
// Notes:
//   - Flash can only change bits from 1 to 0, not 0 to 1
//   - Erase sets all bits to 1 (0xFF) in 4KB sectors
//   - Sector size: 0x1000 (4096 bytes)
//
void PY25Q16_WriteBuffer(uint32_t Address, const void *pBuffer, uint32_t Size, bool Append)
{
#ifdef DEBUG
    printf("spi flash write: %06x %ld %d\n", Address, Size, Append);
#endif
    uint32_t SecIndex = Address / SECTOR_SIZE;
    uint32_t SecAddr = SecIndex * SECTOR_SIZE;
    uint32_t SecOffset = Address % SECTOR_SIZE;
    uint32_t SecSize = SECTOR_SIZE - SecOffset;

    while (Size)
    {
        if (Size < SecSize)
        {
            SecSize = Size;
        }

        if (SecAddr != SectorCacheAddr)
        {
            PY25Q16_ReadBuffer(SecAddr, SectorCache, SECTOR_SIZE);
            SectorCacheAddr = SecAddr;
        }

        if (0 != memcmp(pBuffer, (char *)SectorCache + SecOffset, SecSize))
        {
            bool Erase = false;
            for (uint32_t i = 0; i < SecSize; i++)
            {
                if (0xff != SectorCache[SecOffset + i])
                {
                    Erase = true;
                    break;
                }
            }

            memcpy(SectorCache + SecOffset, pBuffer, SecSize);

            if (Erase)
            {
                SectorErase(SecAddr);
                if (Append)
                {
                    SectorProgram(SecAddr, SectorCache, SecOffset + SecSize);
                    memset(SectorCache + SecOffset + SecSize, 0xff, SECTOR_SIZE - SecOffset - SecSize);
                }
                else
                {
                    SectorProgram(SecAddr, SectorCache, SECTOR_SIZE);
                }
            }
            else
            {
                SectorProgram(Address, pBuffer, SecSize);
            }
        }

        Address += SecSize;
        pBuffer += SecSize;
        Size -= SecSize;

        SecAddr += SECTOR_SIZE;
        SecOffset = 0;
        SecSize = SECTOR_SIZE;
    } // while
}

// ============================================================================
// PY25Q16_SectorErase - Public sector erase with cache invalidation
// ============================================================================
// Erases a 4KB sector and invalidates the sector cache if affected.
// Used for explicit sector erasure when needed.
//
// Parameters:
//   Address: Any address within the 4KB sector (automatically aligned)
//
// Dependencies:
//   - SectorErase() - low-level SPI erase command
//   - SectorCacheAddr, SectorCache[] - static cache for optimization
//   - SECTOR_SIZE (0x1000) - 4KB sector boundary
//
// Timing:
//   - Erase operation: ~50-100ms
//   - Cache invalidation: negligible
//   - Total: ~50-100ms (blocking)
//
// Notes:
//   - Sets all bits to 1 (0xFF) in the 4KB sector
//   - Address is automatically aligned down to sector boundary
//   - Cache is flushed to prevent stale data
//
void PY25Q16_SectorErase(uint32_t Address)
{
    Address -= (Address % SECTOR_SIZE);
    SectorErase(Address);
    if (SectorCacheAddr == Address)
    {
        memset(SectorCache, 0xff, SECTOR_SIZE);
    }
}

// ============================================================================
// WriteAddr - Transmit 24-bit address via SPI (big-endian)
// ============================================================================
// Sends 3-byte address to flash chip per SPI flash protocol (MSB first).
//
// Parameters:
//   Addr: 24-bit address (0x000000 - 0x1FFFFF)
//
// Dependencies:
//   - SPI_WriteByte() - low-level SPI transmission
//
// Timing:
//   - 3 bytes @ ~1us each ≈ 3-5us total
//
static inline void WriteAddr(uint32_t Addr)
{
    SPI_WriteByte(0xff & (Addr >> 16));
    SPI_WriteByte(0xff & (Addr >> 8));
    SPI_WriteByte(0xff & Addr);
}

// ============================================================================
// ReadStatusReg - Read flash status register (0, 1, or 2)
// ============================================================================
// Reads one of three status registers to check flash state.
// Register 0: WIP (bit 0), WEL (bit 1), BP[2:0] (bits 4-2), etc.
// Register 1-2: Reserved/manufacturer specific
//
// Parameters:
//   Which: Register index (0=Status 1, 1=Status 2, 2=Status 3)
//
// Dependencies:
//   - SPI_WriteByte() - SPI communication
//   - CS_Assert(), CS_Release() - chip select control
//   - Commands: 0x05, 0x35, 0x15 (SPI flash standard)
//
// Timing:
//   - Read cycle: ~5-10us
//
// Returns:
//   Status byte value (0 if invalid register)
//
static uint8_t ReadStatusReg(uint32_t Which)
{
    uint8_t Cmd;
    switch (Which)
    {
    case 0:
        Cmd = 0x5;
        break;
    case 1:
        Cmd = 0x35;
        break;
    case 2:
        Cmd = 0x15;
        break;
    default:
        return 0;
    }

    CS_Assert();
    SPI_WriteByte(Cmd);
    uint8_t Value = SPI_WriteByte(0xff);
    CS_Release();

    return Value;
}

// ============================================================================
// WaitWIP - Poll status register until write operation completes
// ============================================================================
// Blocks until WIP (Write In Progress) bit clears in status register 0.
// Used after erase, write enable, or program operations.
//
// Dependencies:
//   - ReadStatusReg(0) - status register polling
//   - SYSTICK_DelayUs() - microsecond delay between polls
//
// Timing:
//   - Typical erase: 50-100ms
//   - Typical program: 1-5ms
//   - Poll interval: 10us between checks
//   - Timeout: ~10 seconds (1,000,000 iterations x 10us)
//   - Blocking; CPU busy-waits
//
// Notes:
//   - Critical for data integrity; must wait for flash operations
//   - Long timeout protects against flash command failures
//
static void WaitWIP()
{
    for (int i = 0; i < 1000000; i++)
    {
        uint8_t Status = ReadStatusReg(0);
        if (1 & Status) // WIP
        {
            SYSTICK_DelayUs(10);
            continue;
        }
        break;
    }
}

// ============================================================================
// WriteEnable - Set Write Enable Latch (WEL) bit in status register
// ============================================================================
// Sends WREN command to enable write/erase operations.
// Must be called before any program or erase command.
//
// Dependencies:
//   - SPI_WriteByte() - command transmission
//   - CS_Assert(), CS_Release() - chip select
//   - Command: 0x06 (SPI flash standard WREN)
//
// Timing:
//   - ~5us
//
// Notes:
//   - WEL bit automatically clears after write/erase completes
//   - Must be called for each write/erase operation
//
static void WriteEnable()
{
    CS_Assert();
    SPI_WriteByte(0x6);
    CS_Release();
}

// ============================================================================
// SectorErase - Erase a 4KB sector (low-level SPI command)
// ============================================================================
// Sends SE (Sector Erase) command 0x20 to flash chip.
// Erases entire 4KB sector, setting all bits to 1 (0xFF).
//
// Parameters:
//   Addr: Starting address of 4KB sector
//
// Dependencies:
//   - WriteEnable() - must enable writes first
//   - WaitWIP() - must wait for previous op to complete and for erase to finish
//   - WriteAddr() - address transmission
//   - SPI_WriteByte(), CS_Assert(), CS_Release() - SPI control
//   - Command: 0x20 (SPI flash standard SE)
//
// Timing:
//   - Pre-erase setup: WriteEnable() ~5us + WaitWIP() ~1us
//   - Erase command: ~10us
//   - Erase operation: ~50-100ms (longest part)
//   - Post-erase: WaitWIP() ~50-100ms
//   - Total: ~100-200ms (blocking)
//
// Notes:
//   - Highest power consumption during erase
//   - Must wait for completion before next operation
//
static void SectorErase(uint32_t Addr)
{
#ifdef DEBUG
    printf("spi flash sector erase: %06x\n", Addr);
#endif
    WaitWIP();  // calypso marker CRITICAL: Wait for any previous operation to complete before issuing WriteEnable
    WriteEnable();
    
    
    CS_Assert();
    SPI_WriteByte(0x20);
    WriteAddr(Addr);
    CS_Release();

    SYSTICK_DelayUs(150000);  // calypso marker block erase time typical 150ms

    WaitWIP();
}

// ============================================================================
// SectorProgram - Program up to 4KB of data, handling page boundaries
// ============================================================================
// Writes data to flash by programming one 256-byte page at a time.
// Automatically handles crossing page (256B) boundaries.
//
// Parameters:
//   Addr: Starting address within erased sector
//   Buf:  Pointer to source data
//   Size: Bytes to program (1-4096)
//
// Dependencies:
//   - PageProgram() - programs individual 256-byte pages
//   - PAGE_SIZE (0x100) constant - 256-byte page size
//
// Timing:
//   - Per 256-byte page: ~1-5ms
//   - 4KB sector (16 pages): ~16-80ms total
//   - Blocking; waits for each page completion
//
// Notes:
//   - Flash must be erased before programming (bits only go 1→0)
//   - Page programming is required; cannot program arbitrary sizes
//
static void SectorProgram(uint32_t Addr, const uint8_t *Buf, uint32_t Size)
{
    uint32_t Size1 = PAGE_SIZE - (Addr % PAGE_SIZE);

    while (Size)
    {
        if (Size < Size1)
        {
            Size1 = Size;
        }

        PageProgram(Addr, Buf, Size1);

        Addr += Size1;
        Buf += Size1;
        Size -= Size1;

        Size1 = PAGE_SIZE;
    }
}

// ============================================================================
// PageProgram - Program a single 256-byte page (low-level SPI command)
// ============================================================================
// Sends PP (Page Program) command 0x02 to write up to 256 bytes.
// Programs within single page boundary; larger data requires SectorProgram().
//
// Parameters:
//   Addr: Address within page (will wrap if exceeds page boundary)
//   Buf:  Source data pointer
//   Size: Bytes to program (1-256; wraps at page boundary)
//
// Dependencies:
//   - WriteEnable() - enable writes before programming
//   - WaitWIP() - wait for completion after programming
//   - WriteAddr() - send 24-bit address
//   - SPI_WriteBuf() or SPI_WriteByte() - data transmission
//   - CS_Assert(), CS_Release() - chip select control
//   - Command: 0x02 (SPI flash standard PP)
//
// Timing:
//   - Setup: WriteEnable() ~5us
//   - Command: ~10us
//   - Data transmission: ~256us (256 bytes @ ~1us each)
//   - Programming: ~1-5ms (typical 4ms)
//   - Post-program: WaitWIP() ~5ms
//   - Total: ~5-10ms (blocking)
//
// Notes:
//   - Uses DMA for data if Size >= 16 bytes (SPI_WriteBuf)
//   - Uses single-byte SPI writes if Size < 16 bytes
//   - Wraps within 256-byte page if address+size exceeds boundary
//   - Flash must be erased before programming
//
static void PageProgram(uint32_t Addr, const uint8_t *Buf, uint32_t Size)
{
#ifdef DEBUG
    printf("spi flash page program: %06x %ld\n", Addr, Size);
#endif

    WaitWIP();  // calypso marker CRITICAL: Wait for any previous operation to complete before issuing WriteEnable
    WriteEnable();

    
    CS_Assert();

    SPI_WriteByte(0x2);
    WriteAddr(Addr);

    if (Size >= 16)
    {
        SPI_WriteBuf(Buf, Size);
    }
    else
    {
        for (uint32_t i = 0; i < Size; i++)
        {
            SPI_WriteByte(Buf[i]);
        }
    }

    CS_Release();

    SYSTICK_DelayUs(400000);  // calypso marker page program time typical 400ms

    WaitWIP();
}

// ============================================================================
// DMA1_Channel4_5_6_7_IRQHandler - DMA transfer complete interrupt handler
// ============================================================================
// Handles DMA transfer completion for SPI read/write operations.
// Sets TC_Flag to true when DMA channel 4 (RX) finishes transfer.
// Ensures SPI FIFOs are empty before signaling completion.
//
// Dependencies:
//   - DMA1 channel 4 (SPI RX)
//   - SPI2 peripheral
//   - TC_Flag (volatile bool) - global flag for transfer completion
//
// Timing:
//   - ISR entry: immediate on DMA TC
//   - FIFO checks: ~1-5us
//   - Flag set: immediate
//   - Total: ~5-10us
//
// Notes:
//   - TC_Flag is initialized to false in SPI_ReadBuf/SPI_WriteBuf
//   - While (!TC_Flag) loop in those functions waits for this flag
//   - Ensures all SPI data is processed before signaling done
//
void DMA1_Channel4_5_6_7_IRQHandler()
{
    if (LL_DMA_IsActiveFlag_TC4(DMA1) && LL_DMA_IsEnabledIT_TC(DMA1, CHANNEL_RD))
    {
        LL_DMA_DisableIT_TC(DMA1, CHANNEL_RD);
        LL_DMA_ClearFlag_TC4(DMA1);

        while (LL_SPI_TX_FIFO_EMPTY != LL_SPI_GetTxFIFOLevel(SPIx))
            ;
        while (LL_SPI_IsActiveFlag_BSY(SPIx))
            ;
        while (LL_SPI_RX_FIFO_EMPTY != LL_SPI_GetRxFIFOLevel(SPIx))
            ;

        LL_SPI_DisableDMAReq_TX(SPIx);
        LL_SPI_DisableDMAReq_RX(SPIx);

        TC_Flag = true;
    }
}
