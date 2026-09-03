#include "FreeRTOS.h"

#include "bcm2712_smp.h"
#include "bcm2712_timer.h"
#include "interrupt.h"
#include "rp1.h"
#include "rp1_uart0.h"
#include "task.h"

#include <stdint.h>

void __real_main(void);

void __wrap_main(void)
{
#if configNUMBER_OF_CORES > 1
    bcm2712SmpBringupSecondaryCpus();
#endif

    __real_main();

    for (;;)
    {
    }
}

void vApplicationIRQHandler(uint32_t iar)
{
    uint32_t irqId = iar & 0x3ffu;

    if (irqId == CNTP_IRQ30)
    {
        FreeRTOS_Tick_Handler();
    }
    else if (irqId == RP1_GPIO_GIC_INTID)
    {
        compatibilityInterruptDispatch();
    }
}

void vDebugSmpRecordSwitchIn(uint32_t core)
{
    (void)core;
}

void vDebugSmpRecordTick(uint32_t core)
{
    (void)core;
}

void vDebugSmpRecordIrq(uint32_t core, uint32_t irqId)
{
    (void)core;
    (void)irqId;
}

#if configCHECK_FOR_STACK_OVERFLOW > 0
void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName)
{
    (void)task;

    rp1Uart0Puts("\n[STACK OVERFLOW] task=");
    rp1Uart0Puts(taskName != 0 ? taskName : "(null)");
    rp1Uart0Puts("\n");

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
#endif
