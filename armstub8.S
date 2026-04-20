#include "armstub8_arch.h"

/*
 * place the magic values patched by rpi VideoCore firmware at the header partition
 */
.section .hdr, "ax"
.global _start

_start:
    b armstub8_start

.org 0xD4
    .ascii "FIQS"
.globl spin_cpu0
spin_cpu0:
    .quad 0
.globl spin_cpu1
spin_cpu1:
    .quad 0
.globl spin_cpu2
spin_cpu2:
    .quad 0
.globl spin_cpu3
spin_cpu3:
magic:
    .word 0x5afe570b
version:
    .word 0x0
dtb_ptr32:
    .word 0x0
kernel_entry32:
    .word 0x0

.org 0x1000
armstub8_start:

/*
 * armstub8 start simulating bl31 running at el3, setting environment and jumping to el2
 */
.section .text
.global armstub8_entrypoint

armstub8_entrypoint:

        mov x0, SCR_VAL
        msr SCR_EL3, x0

        ldr x0, =0x30c50830
        msr SCTLR_EL2, x0

        mov x0, SPSR_EL3_VAL
        msr spsr_el3, x0
        adr x0, in_el2
        msr elr_el3, x0
        eret
in_el2:

        mrs x6, MPIDR_EL1
        lsr x6, x6, #8
        and x6, x6, #0x3
        cbz x6, primary_cpu

        adr x5, spin_cpu0
secondary_spin:
        wfe
        ldr x4, [x5, x6, lsl #3]
        cbz x4, secondary_spin
        mov x0, #0
        b boot_kernel

primary_cpu:
        ldr w4, kernel_entry32

boot_kernel:
        mov x1, #0
        mov x2, #0
        mov x3, #0
        br x4

