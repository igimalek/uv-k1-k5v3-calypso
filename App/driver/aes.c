 

#include <stdbool.h>

#include "bsp/dp32g030/aes.h"
#include "driver/aes.h"

static void AES_Setup_ENC_CBC(bool IsDecrypt, const void *pKey, const void *pIv)
{
    const uint32_t *pK = (const uint32_t *)pKey;
    const uint32_t *pI = (const uint32_t *)pIv;

    (void)IsDecrypt;     
    
    AES_CR = (AES_CR & ~AES_CR_EN_MASK) | AES_CR_EN_BITS_DISABLE;
    AES_CR = AES_CR_CHMOD_BITS_CBC;
    AES_KEYR3 = pK[0];
    AES_KEYR2 = pK[1];
    AES_KEYR1 = pK[2];
    AES_KEYR0 = pK[3];
    AES_IVR3 = pI[0];
    AES_IVR2 = pI[1];
    AES_IVR1 = pI[2];
    AES_IVR0 = pI[3];
    AES_CR = (AES_CR & ~AES_CR_EN_MASK) | AES_CR_EN_BITS_ENABLE;
}

static void AES_Transform(const void *pIn, void *pOut)
{
    const uint32_t *pI = (const uint32_t *)pIn;
    uint32_t *pO = (uint32_t *)pOut;

    AES_DINR = pI[0];
    AES_DINR = pI[1];
    AES_DINR = pI[2];
    AES_DINR = pI[3];

    int timeout = 10000;
    while ((AES_SR & AES_SR_CCF_MASK) == AES_SR_CCF_BITS_NOT_COMPLETE && timeout--) {
    }

    pO[0] = AES_DOUTR;
    pO[1] = AES_DOUTR;
    pO[2] = AES_DOUTR;
    pO[3] = AES_DOUTR;

    AES_CR |= AES_CR_CCFC_BITS_SET;
}

void AES_Encrypt(const void *pKey, const void *pIv, const void *pIn, void *pOut, uint8_t NumBlocks)
{
    const uint8_t *pI = (const uint8_t *)pIn;
    uint8_t *pO = (uint8_t *)pOut;
    uint8_t i;

    AES_Setup_ENC_CBC(0, pKey, pIv);
    for (i = 0; i < NumBlocks; i++) {
        AES_Transform(pI + (i * 16), pO + (i * 16));
    }
}

