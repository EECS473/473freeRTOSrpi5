#ifndef BCM2712_H_
#define BCM2712_H_

#include "reg.h"
#include <stdint.h>

#define BCM2712_PER_BASE 0x1000000000ULL

#define UART10_BASE (BCM2712_PER_BASE + 0x7D001000ULL)
#define UART_DR (*(volatile uint32_t*)(UART10_BASE + 0x00))
#define UART_FR (*(volatile uint32_t*)(UART10_BASE + 0x18))
#define UART_IBRD (*(volatile uint32_t*)(UART10_BASE + 0x24))
#define UART_FBRD (*(volatile uint32_t*)(UART10_BASE + 0x28))
#define UART_LCR_H (*(volatile uint32_t*)(UART10_BASE + 0x2C))
#define UART_CR (*(volatile uint32_t*)(UART10_BASE + 0x30))
#define UART_FR_TXFF (1 << 5)

#define GICD_BASE (BCM2712_PER_BASE + 0x7FFF9000ULL)
#define GICD_CTLR (*(volatile uint32_t*)(GICD_BASE + 0x0000))
#define GICD_ISENABLER(n) (*(volatile uint32_t*)(GICD_BASE + 0x0100 + (n) / 32 * 4))
#define GICD_ISPENDR(n) (*(volatile uint32_t*)(GICD_BASE + 0x200 + (n) / 32 * 4))
#define GICD_IPRIORITYR(n) (*(volatile uint32_t*)(GICD_BASE + 0x0400 + (n) / 4 * 4))
#define GICD_ITARGETSR(n) (*(volatile uint32_t*)(GICD_BASE + 0x0800 + (n) / 4 * 4))

#define GICC_BASE (BCM2712_PER_BASE + 0x7FFFA000ULL)
#define GICC_CTLR (*(volatile uint32_t*)(GICC_BASE + 0x0000))
#define GICC_PMR (*(volatile uint32_t*)(GICC_BASE + 0x0004))
#define GICC_IAR (*(volatile uint32_t*)(GICC_BASE + 0x000C))
#define GICC_EOIR (*(volatile uint32_t*)(GICC_BASE + 0x0010))

#define GIO_BASE (BCM2712_PER_BASE + 0x7D517C00ULL)
#define GIO_DATA (*(volatile uint32_t*)(GIO_BASE + 0x04))

#endif
