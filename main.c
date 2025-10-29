#include "bcm2712_gpio.h"
#include "bcm2712_timer.h"
#include "bcm2712_uart10.h"
#include <stdint.h>
#include <stdio.h>

void print_registers_el2(void)
{
    uint64_t elr_el2, spsr_el2, sctlr_el2, sp, daif, sp_el1;

    asm volatile("mrs %0, elr_el2" : "=r"(elr_el2));
    asm volatile("mrs %0, spsr_el2" : "=r"(spsr_el2));
    asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr_el2));
    asm volatile("mov %0, sp" : "=r"(sp));
    asm volatile("mrs %0, daif" : "=r"(daif));
    asm volatile("mrs %0, sp_el1" : "=r"(sp_el1));

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

int main(void)
{
    vSetup1msTickInterrupt();

    while (1)
    {
    }
}

void freertos_tick_handler(void)
{
    static uint64_t ticks = 0ull;
    uint32_t irq_id = GICC_IAR;
    if (++ticks % 1000 == 0)
    {
        uart10Puts("[" __FILE__ "]"
                   ": 1ms tick counter: 0x");
        uart10PrintHex64(ticks);
        uart10Puts(" : interrupt request id: 0x");
        uart10PrintfHex(irq_id);
        uart10Puts("\n");
    }
    vSetup1msInterruptNextTick();
    GICC_EOIR = irq_id;
}
