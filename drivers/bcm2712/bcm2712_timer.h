#ifndef BCM2712_TIMER_H_
#define BCM2712_TIMER_H_

#include <stdint.h>

#define CNTP_IRQ30 (14 + 16) /* PPI14 from bcm2712.dtsi */
#define TICK_INTERVAL_MS 1

void timerInit(void);
void timerWait(uint32_t ms);
void timerWaitUs(uint32_t us);
uint64_t timerGetMicroseconds(void);
void vSetupTickInterrupt(void);
void vResetTickInterrupt(void);
void vPortSetupTickInterruptSecondary(void);

#endif
