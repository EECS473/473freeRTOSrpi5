#include "FreeRTOS.h"
#include "bcm2712_gpio.h"
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

static void exampleTask100ms(void* /* parameters */)
{
    for (;;)
    {
        vTaskDelay(100);
        uart10Puts("Tick count (ms): ");
        uart10PrintDec(xTaskGetTickCount());
        uart10Puts("\n");
    }
}

static void exampleTask1s(void* /* parameters */)
{
    for (;;)
    {
        vTaskDelay(1000);
        uart10Puts("example task 1s cyclic\n");
    }
}

int main(void)
{
    static StaticTask_t exampleTaskTCB __attribute__((aligned(16)));
    static StackType_t exampleTaskStack[configMINIMAL_STACK_SIZE] __attribute__((aligned(16)));

    (void)xTaskCreateStatic(exampleTask100ms,
                            "exampleTask100ms",
                            configMINIMAL_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1U,
                            &exampleTaskStack[0],
                            &exampleTaskTCB);

    TaskHandle_t exampleTask1sHandle = NULL;

    (void)xTaskCreate(exampleTask1s,
                      "exampleTask1s",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      configMAX_PRIORITIES - 1U,
                      &exampleTask1sHandle);

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
