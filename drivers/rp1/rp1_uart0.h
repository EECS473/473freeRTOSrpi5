#ifndef RP1_UART0_H_
#define RP1_UART0_H_

#include <stdint.h>

/*
 * Raspberry Pi 5 RP1 UART0 console.
 *
 * RP1 UART0 is located at: 
 *
 *     GPIO14 / physical pin 8  = TX
 *     GPIO15 / physical pin 10 = RX
 *
 * The boot firmware initializes the UART when config.txt contains
 * enable_uart=1 and enable_rp1_uart=1, so rp1Uart0Init() intentionally does
 * not reprogram the PL011 clock or baud-rate registers.
 */

void rp1Uart0Init(void);
void rp1Uart0Putc(char c);
void rp1Uart0Puts(const char *s);
void rp1Uart0PrintDec(uint64_t val);
void rp1Uart0PrintHex8(uint8_t val);
void rp1Uart0PrintHex16(uint16_t val);
void rp1Uart0PrintHex32(uint32_t val);
void rp1Uart0PrintHex64(uint64_t val);

#endif /* RP1_UART0_H_ */
