#ifndef BCM2712_UART10_H_
#define BCM2712_UART10_H_

#include "bcm2712.h"

void uart10Init(void);

void uart10Putc(char c);

void uart10Puts(const char* s);

void uart10PrintDec(uint64_t val);

void uart10PrintHex8(uint8_t data);

void uart10PrintHex32(uint32_t val);

void uart10PrintHex64(uint64_t val);

#endif
