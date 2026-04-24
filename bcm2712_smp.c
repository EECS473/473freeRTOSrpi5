#include "FreeRTOS.h"
#include "bcm2712.h"
#include "bcm2712_uart10.h"
#include <stdint.h>

#define SMP_NUM_CORES 4U
#define SMP_YIELD_SGI_ID 0U /* SGI 0 used as inter-core yield IPI, custom defined */

extern void _start(void);

static void smpDsbSy(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static void smpSendEvent(void)
{
    __asm__ volatile("sev" ::: "memory");
}

/*
 * Release CPU1..3 from the armstub holding pen and let them re-enter _start,
 * so they fall through the same EL3->EL2->EL1 chain CPU0 uses.
 *
 * Must be called from CPU0 before vTaskStartScheduler() so that, by the time the woken cores reach
 * vPortStartSchedulerOnSecondaryCore() and start polling ullPortSchedulerRunning,
 * the CPU0-side scheduler init iis on its way.
 */

void bcm2712SmpBringupSecondaryCpus(void)
{
    RPI5_MBOX_ENTRY = (uint64_t)(uintptr_t)&_start;
    smpDsbSy();

    for (uint32_t core = 1u; core < SMP_NUM_CORES; ++core)
    {
        RPI5_MBOX_HOLD(core) = 1u;
    }

    smpSendEvent();
}

/*
 * Send GICv2 SGI 0 to signal a target core. Used by the FreeRTOS port to implement
 * portYIELD_CORE(xCoreID): the targeted core takes the SGI as a normal IRQ,
 * vApplicationIRQHandler() ignores the payload and the IRQ tail-routine notices
 * ullPortYieldRequired[core] (set below) and re-enters the scheduler on exit.
 *
 * GICD_SGIR fields (ARM IHI 0048B):
 * [25:24] TargetListFilter (00 = use [23:16] CPUTargetList)
 * [23:16] CPUTargetList (1 << target_cpu)
 * [15] NSATT (0 in our setup)
 * [3:0] SGIINTID
 */
void vPortYieldCore(uint32_t ulCoreID)
{
    extern uint64_t ullPortYieldRequired[];
    if (ulCoreID >= SMP_NUM_CORES)
    {
        return;
    }

    ullPortYieldRequired[ulCoreID] = pdTRUE;
    smpDsbSy();

    GICD_SGIR = (0U << 24) | ((1U << ulCoreID) << 16) | (SMP_YIELD_SGI_ID & 0xFu);
    smpDsbSy();
}