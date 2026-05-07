#include "bcm2712_timer.h"
#include "FreeRTOS.h"
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
    GICD_CTLR = 0x03;
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
    pri |= (((configMAX_API_CALL_INTERRUPT_PRIORITY - 2) << portPRIORITY_SHIFT) << ((CNTP_IRQ30 % 4) * 8));
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

    {
        uint64_t mpidr;
        __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
        uint32_t coreid = (uint32_t)((mpidr >> 8) & 0xFF);

        uint32_t igroup0 = *(volatile uint32_t*)(0x107FFF9080);
        uint32_t isenable0 = *(volatile uint32_t*)(0x107FFF9100);

        uint32_t gicc_ctlr = GICC_CTLR;
        uint32_t gicc_pmr = GICC_PMR;
        uint32_t cntp_ctl_now = (uint32_t)SYSREG_READ(CNTP_CTL_EL0);
        uint64_t cntpct = SYSREG_READ(CNTPCT_EL0);
        uint64_t cntp_cv = SYSREG_READ(CNTP_CVAL_EL0);
        uint64_t daif;
        __asm__ volatile("mrs %0, daif" : "=r"(daif));

        uart10Puts("[gic-state] core=");
        uart10PrintDec(coreid);
        uart10Puts(" IGROUP0=0x");
        uart10PrintHex32(igroup0);
        uart10Puts(" ISENABLER0=0x");
        uart10PrintHex32(isenable0);
        uart10Puts(" GICC_CTLR=0x");
        uart10PrintHex32(gicc_ctlr);
        uart10Puts(" PMR=0x");
        uart10PrintHex32(gicc_pmr);
        uart10Puts(" CNTP_CTL=0x");
        uart10PrintHex32((uint32_t)(cntp_ctl_now));
        uart10Puts(" daif=0x");
        uart10PrintHex32((uint32_t)daif);
        uart10Puts("\n CNTPCT=0x");
        uart10PrintHex64(cntpct);
        uart10Puts(" CVAL=0x");
        uart10PrintHex64(cntp_cv);
        uart10Puts(" delta=");
        uart10PrintDec((uint32_t)(cntp_cv - cntpct));
        uart10Puts("\n");
    }
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
