#include "bcm2712_timer.h"
#include "bcm2712.h"
#include "bcm2712_uart10.h"
#include "reg.h"
#include <stdatomic.h>
#include <stdint.h>

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


/*
 * BCM2712 timer / GIC bring-up
 *
 * The CNTP physical timer interrupt (PPI30, private peripheral interrupt) is per-CPU,
 * so each core has to enable its own copy. We split the original do-it-all
 * vSetupTickInterrupt() into:
 * - vSetupTickInterruptDistributor(): runs on CPU0 only, deals with SPI range and turns
 *   the distributor on.
 * - vSetupTickInterruptPerCore(): runs on every core, programs the banked PPI register,
 *   the core's GIC CPU interface and starts CNTP.
 * vSetupTickInterrupt() (called by configSETUP_TICK_INTERRUPT) keeps the old single-call
 * semantics on the boot core.
 */
static uint64_t ticks;
static _Atomic uint32_t s_distributorReady = 0u;

static void vSetupTickInterruptDistributor(void)
{
    uint32_t frq = SYSREG_READ(CNTFRQ_EL0);
    ticks = frq / 1000;
    GICD_CTLR = 0x00;
    GICD_CTLR = 0x01;
    __asm__ volatile("dsb sy" ::: "memory");
    atomic_store_explicit(&s_distributorReady, 1u, memory_order_release);
    __asm__ volatile("sev" ::: "memory");
}

static void vSetupTickInterruptPerCore(void)
{

    GICD_IGROUPR(0) = 0xFFFFFFFFu;
    GICD_ISENABLER(CNTP_IRQ30) |= (1U << (CNTP_IRQ30 % 32));

    uint32_t pri = GICD_IPRIORITYR(CNTP_IRQ30);
    pri &= ~(0xFFu << ((CNTP_IRQ30 % 4) * 8));
    pri |= ((14 << 4) << ((CNTP_IRQ30 % 4) * 8));
    GICD_IPRIORITYR(CNTP_IRQ30) = pri;

    GICC_CTLR = 0x00;
    GICC_PMR = 0xFF;
    GICC_BPR = 0x00;
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
}

void vSetupTickInterrupt(void)
{
    vSetupTickInterruptDistributor();
    vSetupTickInterruptPerCore();
}

void vPortSetupTickInterruptSecondary(void)
{
    while (atomic_load_explicit(&s_distributorReady, memory_order_acquire) == 0u)
    {
        __asm__ volatile("wfe" ::: "memory");
    }
    vSetupTickInterruptPerCore();
}

void vResetTickInterrupt(void)
{
    uint64_t current_cval = SYSREG_READ(CNTP_CVAL_EL0);
    SYSREG_WRITE(CNTP_CVAL_EL0, current_cval + ticks);
}
