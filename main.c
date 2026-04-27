#include "FreeRTOS.h"
#include "bcm2712_gpio.h"
#include "bcm2712_smp.h"
#include "bcm2712_timer.h"
#include "bcm2712_uart10.h"
#include "task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void print_registers_el2(void)
{
    uint64_t elr_el2, spsr_el2, sctlr_el2, sp, daif, sp_el1;

    __asm__ volatile("mrs %0, elr_el2" : "=r"(elr_el2));
    __asm__ volatile("mrs %0, spsr_el2" : "=r"(spsr_el2));
    __asm__ volatile("mrs %0, sctlr_el2" : "=r"(sctlr_el2));
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("mrs %0, sp_el1" : "=r"(sp_el1));

    uart10Puts("elr_el2: 0x");
    uart10PrintHex64(elr_el2);
    uart10Puts("\n");

    uart10Puts("spsr_el2: 0x");
    uart10PrintHex64(spsr_el2);
    uart10Puts("\n");

    uart10Puts("sctlr_el2: 0x");
    uart10PrintHex64(sctlr_el2);
    uart10Puts("\n");

    uart10Puts("sp: 0x");
    uart10PrintHex64(sp);
    uart10Puts("\n");

    uart10Puts("daif: 0x");
    uart10PrintHex64(daif);
    uart10Puts("\n");

    uart10Puts("sp_el1: 0x");
    uart10PrintHex64(sp_el1);
    uart10Puts("\n");
}

int _write_r(struct _reent* r, void* fd, const char* buf, int nbytes);

void syscall_pre(void)
{
    stdout->_write = _write_r;
}

static inline uint32_t currentCoreId(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)((mpidr >> 8) & 0xFF);
}

static void exampleTask100ms(void* /* parameters */)
{
    for (;;)
    {
        vTaskDelay(100);
        uart10Puts("[core ");
        uart10PrintDec(currentCoreId());
        uart10Puts("] tick (ms): ");
        uart10PrintDec(xTaskGetTickCount());
        uart10Puts("\n");
    }
}

static void exampleTask1s(void* /* parameters */)
{
    for (;;)
    {
        vTaskDelay(1000);
        uart10Puts("[core ");
        uart10PrintDec(currentCoreId());
        uart10Puts("] 1s cyclic\n");
    }
}

static void coreHeartbeatTask(void* parameters)
{
    uint32_t myAffinityCore = (uint32_t)(uintptr_t)parameters;
    for (;;)
    {
        vTaskDelay(500);
        uart10Puts("hb: pinned-core=");
        uart10PrintDec(myAffinityCore);
        uart10Puts(" running-core=");
        uart10PrintDec(currentCoreId());
        uart10Puts("\n");
    }
}

int main(void)
{
    uart10Puts("[core 0] FreeRTOS SMP boot\n");
    uart10Puts("[core 0] FreeRTOS SMP boot\n");
    TaskHandle_t exampleTask100msHandle = NULL;
    (void)xTaskCreate(exampleTask100ms,
                      "exampleTask100ms",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      configMAX_PRIORITIES - 1U,
                      &exampleTask100msHandle);

    uart10Puts("Before start scheduler \n");
    TaskHandle_t exampleTask1sHandle = NULL;

    (void)xTaskCreate(exampleTask1s,
                      "exampleTask1s",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      configMAX_PRIORITIES - 1U,
                      &exampleTask1sHandle);

#if configNUMBER_OF_CORES > 1
    for (uintptr_t i = 0; i < 4; ++i)
    {
        TaskHandle_t hb = NULL;
        (void)xTaskCreate(coreHeartbeatTask, "hb", configMINIMAL_STACK_SIZE, (void*)i, tskIDLE_PRIORITY + 1u, &hb);
    }
    bcm2712SmpBringupSecondaryCpus();
#endif

    vTaskStartScheduler();

    while (1)
    {
    }
}

void vApplicationIRQHandler(uint32_t ullICCIAR)
{
    uint32_t irq_id = ullICCIAR & 0x3FF;

    if (irq_id == CNTP_IRQ30)
    {
        FreeRTOS_Tick_Handler();
    }
}


#if configCHECK_FOR_STACK_OVERFLOW > 0

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    /* Check pcTaskName for the name of the offending task,
     * or pxCurrentTCB if pcTaskName has itself been corrupted. */
    (void)xTask;
    (void)pcTaskName;
}

#endif
