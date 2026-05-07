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
    uint32_t myAffinityCore __attribute__ ((unused)) = (uint32_t)(uintptr_t)parameters;
    for (;;)
    {
        vTaskDelay(500);
        uart10Puts(" running-core=");
        uart10PrintDec(currentCoreId());
        uart10Puts("\n");
    }
}

static volatile uint64_t ulDbgTick[configNUMBER_OF_CORES];
static volatile uint64_t ulDbgIrq[configNUMBER_OF_CORES];
static volatile uint64_t ulDbgIpi[configNUMBER_OF_CORES];
static volatile uint64_t ulDbgSwIn[configNUMBER_OF_CORES];
static char cDbgTask[configNUMBER_OF_CORES][configMAX_TASK_NAME_LEN];

void vDebugSmpRecordTick(uint32_t core)
{
    if (core < configNUMBER_OF_CORES)
    {
        ulDbgTick[core]++;
    }
}

void vDebugSmpRecordIrq(uint32_t core, uint32_t irqId)
{
    if (core > configNUMBER_OF_CORES) return;

    ulDbgIrq[core]++;

    if (irqId == 0u)
    {
        ulDbgIpi[core]++;
    }
}

void vDebugSmpRecordSwitchIn(uint32_t core)
{
    if (core > configNUMBER_OF_CORES) return;
    ulDbgSwIn[core]++;
    const char* name = pcTaskGetName(NULL);
    int i;
    for (i = 0; i < configMAX_TASK_NAME_LEN - 1 && name && name[i]; ++i)
    {
        cDbgTask[core][i] = name[i];
    }
    cDbgTask[core][i] = '\0';
}

static void vDebugSmpDuperTask(void* p)
{
    (void)p;
    uint64_t prevT[configNUMBER_OF_CORES] = {0};
    uint64_t prevR[configNUMBER_OF_CORES] = {0};
    uint64_t prevP[configNUMBER_OF_CORES] = {0};
    uint64_t prevS[configNUMBER_OF_CORES] = {0};

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        for (uint32_t i = 0; i < configNUMBER_OF_CORES; ++i)
        {
            uint64_t t = ulDbgTick[i], dt = t - prevT[i]; prevT[i] = t;
            uint64_t r = ulDbgIrq[i], dr = r - prevR[i]; prevR[i] = r;
            uint64_t y = ulDbgIpi[i], dy = y - prevP[i]; prevP[i] = y;
            uint64_t s = ulDbgSwIn[i], ds = s - prevS[i]; prevS[i] = s;

            uart10Puts("[smp] core=");
            uart10PrintDec(i);
            uart10Puts(" tick/s="); uart10PrintDec(dt);
            uart10Puts(" irq/s="); uart10PrintDec(dr);
            uart10Puts(" yieldIpi/s="); uart10PrintDec(dy);
            uart10Puts(" swIn/s="); uart10PrintDec(ds);
            uart10Puts(" curTask=");
            uart10Puts(cDbgTask[i][0] ? cDbgTask[i] : "<none>");
            uart10Puts("\n");
        }

        uart10Puts("\n");
    }
}



int main(void)
{
    TaskHandle_t exampleTask100msHandle = NULL;
    (void)xTaskCreate(exampleTask100ms,
                      "exampleTask100ms",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      configMAX_PRIORITIES - 1U,
                      &exampleTask100msHandle);

    TaskHandle_t exampleTask1sHandle = NULL;

    (void)xTaskCreate(exampleTask1s,
                      "exampleTask1s",
                      configMINIMAL_STACK_SIZE,
                      NULL,
                      configMAX_PRIORITIES - 1U,
                      &exampleTask1sHandle);

#if configNUMBER_OF_CORES > 1
    for (uintptr_t i = 0; i < configNUMBER_OF_CORES; ++i)
    {
        TaskHandle_t hb = NULL;
        (void)xTaskCreate(coreHeartbeatTask, "hb", configMINIMAL_STACK_SIZE, (void*)i, tskIDLE_PRIORITY + 1u, &hb);
    }

    {
        TaskHandle_t hDump = NULL;
        (void)xTaskCreate(vDebugSmpDuperTask, "dump", configMINIMAL_STACK_SIZE * 2, NULL, configMAX_PRIORITIES - 1U, &hDump);
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

    if (irq_id < 1020u)
    {
        vDebugSmpRecordIrq(currentCoreId(), irq_id);
    }

    if (irq_id == CNTP_IRQ30)
    {
        /*
        if (currentCoreId() == 1) 
        {
            uart10Puts("*");
        }
        else if (currentCoreId() == 2)
        {
            uart10Puts("&");
        }
        else if (currentCoreId() == 3)
        {
            uart10Puts("^");
        }
        else
        {
        }
        */
        FreeRTOS_Tick_Handler();
    }
}


#if configCHECK_FOR_STACK_OVERFLOW > 0

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    /* Check pcTaskName for the name of the offending task,
     * or pxCurrentTCB if pcTaskName has itself been corrupted. */
    (void)xTask;
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    uint32_t coreId = (uint32_t)((mpidr >> 8) & 0xff);
    uart10Puts("[STACK OVERFLOW] task=");
    uart10Puts(pcTaskName ? pcTaskName : "(null)");
    uart10Puts(" core=");
    uart10PrintDec(coreId);
    uart10Puts("\n");
    taskDISABLE_INTERRUPTS();
    for (;;)
    ;
}

#endif
