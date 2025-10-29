#include "bcm2712_uart10.h"

void uart10Init(void)
{
    UART_CR = 0x0;
    UART_IBRD = 5;
    UART_FBRD = 0;

    UART_LCR_H = (3 << 5);
    UART_CR = (1 << 9) | (1 << 8) | (1 << 0);
}

void uart10Putc(char c)
{
    while (UART_FR & UART_FR_TXFF)
        ;
    UART_DR = c;
}

void uart10Puts(const char* s)
{
    while (*s != '\0')
    {
        if (*s == '\n')
        {
            uart10Putc('\r');
        }
        uart10Putc(*s++);
    }
}

void uart10PrintfHex(uint8_t data)
{
    uint8_t high = (data >> 4) & 0x0F;
    uart10Putc(high < 10 ? '0' + high : 'A' + (high - 10));

    uint8_t low = data & 0x0F;
    uart10Putc(low < 10 ? '0' + low : 'A' + (low - 10));
}

void uart10PrintHex64(uint64_t val)
{
    const char* hex = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4)
    {
        uart10Putc(hex[(val >> i) & 0xF]);
    }
}
