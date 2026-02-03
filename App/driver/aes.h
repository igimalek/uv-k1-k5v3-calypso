 

#ifndef DRIVER_AES_H
#define DRIVER_AES_H

#include <stdint.h>

void AES_Encrypt(const void *pKey, const void *pIv, const void *pIn, void *pOut, uint8_t NumBlocks);

#endif

