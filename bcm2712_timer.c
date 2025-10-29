#include "bcm2712_timer.h"
#include "bcm2712.h"

void timerInit(void)
{
    uint32_t ctl = SYSREG_READ(CNTP_CTL_EL0);
    if (!(ctl & (1 << 0)))
    {
        SYSREG_WRITE(CNTP_CTL_EL0, ctl | (1 << 0));
    }

    uint64_t start = SYSREG_READ(CNTPCT_EL0);

    while (SYSREG_READ(CNTPCT_EL0) == start)
    {
    }
}


void timerWait(uint32_t ms)
{
    if (ms == 0)
        return;


    uint64_t frq = SYSREG_READ(CNTFRQ_EL0);


    uint64_t ticks_per_ms = frq / 1000;
    uint64_t total_ticks = ticks_per_ms * ms;


    uint64_t start = SYSREG_READ(CNTPCT_EL0);


    while ((SYSREG_READ(CNTPCT_EL0) - start) < total_ticks)
    {
    }
}


static uint64_t ticks;
void vSetup1msTickInterrupt(void)
{
    uint32_t frq = SYSREG_READ(CNTFRQ_EL0);
    ticks = frq / 1000;


    GICD_CTLR = 0x00;
    GICD_ISENABLER(CNTP_IRQ30) |= (1U << (CNTP_IRQ30 % 32));

    uint32_t target = GICD_ITARGETSR(CNTP_IRQ30);
    target &= ~(0xFF << (CNTP_IRQ30 % 4 * 8));
    target |= (1 << (CNTP_IRQ30 % 4 * 8));
    GICD_ITARGETSR(CNTP_IRQ30) = target;

    uint32_t pri = GICD_IPRIORITYR(CNTP_IRQ30);
    pri &= ~(0xFFu << ((CNTP_IRQ30 % 4) * 8));
    pri |= (0x10u << ((CNTP_IRQ30 % 4) * 8));
    GICD_IPRIORITYR(CNTP_IRQ30) = pri;
    GICD_CTLR = 0x01;

    GICC_CTLR = 0x00;
    GICC_PMR = 0xFF;
    GICC_CTLR = 0x01;


    uint32_t cntp_ctl = (uint32_t)SYSREG_READ(CNTP_CTL_EL0);
    cntp_ctl &= ~(1U << 0);
    cntp_ctl |= (1U << 1);
    SYSREG_WRITE(CNTP_CTL_EL0, cntp_ctl);

    uint64_t current_cnt = SYSREG_READ(CNTPCT_EL0);
    SYSREG_WRITE(CNTP_CVAL_EL0, current_cnt + ticks);

    cntp_ctl |= (1U << 0);
    cntp_ctl &= ~(1U << 1);
    cntp_ctl &= ~(1U << 2);
    SYSREG_WRITE(CNTP_CTL_EL0, cntp_ctl);

    __asm__ __volatile__("msr daifclr, #2");
}

void vSetup1msInterruptNextTick(void)
{
    uint64_t current_cval = SYSREG_READ(CNTP_CVAL_EL0);
    SYSREG_WRITE(CNTP_CVAL_EL0, current_cval + ticks);
}
