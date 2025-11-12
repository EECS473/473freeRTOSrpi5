.section .text.start

.global _start
_start:
    ldr x0, =__stack_el2_end
    mov sp, x0

    adrp x0, vector_el2
    add x0, x0, :lo12:vector_el2
    msr vbar_el2, x0
    isb

    ldr x0, =__stack_end
    msr sp_el1, x0

    ldr x0, =next
    msr elr_el2, x0

    mov x0, #0x5
    msr spsr_el2, x0
    isb

    mrs x0, cnthctl_el2
    orr x0, x0, #(1 << 0) | (1 << 1)
    msr cnthctl_el2, x0

    mov x0, #0
    msr hcr_el2, x0

    msr mdcr_el2, xzr
    isb

    /* bl print_registers_el2 */

    eret

.section .text

.align 3
.global next
next:
    adrp x0, _freertos_vector_table
    add x0, x0, :lo12:_freertos_vector_table
    msr vbar_el1, x0
    isb

    /* enable FPEN floating point and SIMD/NEON as memset disassemble: 4e010c20        dup     v0.16b, w1 */
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    isb

    b sysinit

sysinit:
    ldr x0, =__bss_start
    ldr x1, =__bss_end

bss_clear_loop:
    cmp x0, x1
    b.hs bss_clear_done
    str xzr, [x0], #8
    b bss_clear_loop

bss_clear_done:
    bl main

.align 11
.global vector_el2
vector_el2:
    b el2_sync_sp0
    .space 0x80-4
    b el2_irq_sp0
    .space 0x80-4
    b el2_fiq_sp0
    .space 0x80-4
    b el2_serror_sp0
    .space 0x80-4

    b el2_sync_current
    .space 0x80-4
    b el2_irq_current
    .space 0x80-4
    b el2_fiq_current
    .space 0x80-4
    b el2_serror_current
    .space 0x80-4

    b el2_sync_lower
    .space 0x80-4
    b el2_irq_lower
    .space 0x80-4
    b el2_fiq_lower
    .space 0x80-4
    b el2_serror_lower
    .space 0x80-4

    b el2_sync_lower_aarch32
    .space 0x80-4
    b el2_irq_lower_aarch32
    .space 0x80-4
    b el2_fiq_lower_aarch32
    .space 0x80-4
    b el2_serror_lower_aarch32
    .space 0x80-4

el2_sync_sp0:
    b el2_sync_current
el2_irq_sp0:
    b el2_irq_current
el2_fiq_sp0:
    b el2_fiq_current
el2_serror_sp0:
    b el2_serror_current

el2_sync_current:
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mrs x0, esr_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, elr_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, far_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, spsr_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc

    ldp x29, x30, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16

    b .

el2_irq_current:
    b .

el2_fiq_current:
    b .

el2_serror_current:
    b .


el2_sync_lower:
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    mov x0, #0
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc
    mov x0, #0x0d
    bl uart10Putc

    mrs x0, esr_el2
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, elr_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc
    mov x0, #0x0d
    bl uart10Putc

    mrs x0, far_el2
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, spsr_el2
    bl uart10PrintHex64
    mov x0, #0x0a
    bl uart10Putc
    mov x0, #0x0d
    bl uart10Putc

    mrs x0, hcr_el2
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    ldp x29, x30, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16

    b .

el2_irq_lower:
    stp x0, x1, [sp, #-16]!
    mov x0, #0
    bl uart10PrintHex64
    ldp x0, x1, [sp], #16
    b .

el2_fiq_lower:
    b el2_irq_lower

el2_serror_lower:
    stp x0, x1, [sp, #-16]!
    mov x0, #0
    bl uart10PrintHex64
    ldp x0, x1, [sp], #16
    b .

el2_sync_lower_aarch32:
    b el2_sync_lower

el2_irq_lower_aarch32:
    b el2_irq_lower

el2_fiq_lower_aarch32:
    b el2_fiq_lower

el2_serror_lower_aarch32:
    b el2_serror_lower


.align 11
.global _freertos_vector_table
_freertos_vector_table:
    b el1_sync_sp0
    .space 0x80-4
    b el1_irq_sp0
    .space 0x80-4
    b el1_fiq_sp0
    .space 0x80-4
    b el1_serror_sp0
    .space 0x80-4

    b el1_sync_current
    .space 0x80-4
    b el1_irq_current
    .space 0x80-4
    b el1_fiq_current
    .space 0x80-4
    b el1_serror_current
    .space 0x80-4

el1_sync_sp0:
/*
    mov x0, #'S'; bl uart10Putc
    mov x0, #'Y'; bl uart10Putc
    mov x0, #'N'; bl uart10Putc
    mov x0, #'C'; bl uart10Putc
    mov x0, #'_'; bl uart10Putc
    mov x0, #'S'; bl uart10Putc
    mov x0, #'P'; bl uart10Putc
    mov x0, #'0'; bl uart10Putc
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, spsr_el1
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, spsel
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, elr_el1
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

*/

    b FreeRTOS_SWI_Handler

el1_irq_sp0:
/*
    mov x0, #'I'; bl uart10Putc
    mov x0, #'R'; bl uart10Putc
    mov x0, #'Q'; bl uart10Putc
    mov x0, #'_'; bl uart10Putc
    mov x0, #'S'; bl uart10Putc
    mov x0, #'P'; bl uart10Putc
    mov x0, #'0'; bl uart10Putc
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, spsr_el1
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, spsel
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc

    mrs x0, elr_el1
    bl uart10PrintHex64
    mov x0, #'\r'; bl uart10Putc
    mov x0, #'\n'; bl uart10Putc
*/

    b FreeRTOS_IRQ_Handler

el1_fiq_sp0:
    mov x0, #'F'
    bl uart10Putc
    mrs x0, currentel
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc
    b .

el1_serror_sp0:
    mov x0, #'E'
    bl uart10Putc
    mrs x0, currentel
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc
    b .

el1_sync_current:
    mov x0, #'S'
    bl uart10Putc
    mov x0, #'C'
    bl uart10Putc
    mov x0, #':'
    bl uart10Putc
    mov x0, #'\n'
    bl uart10Putc
    mov x0, #'\r'
    bl uart10Putc

    mrs x0, esr_el1; bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    mrs x0, elr_el1; bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc


    mrs x0, far_el1; bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    b .

el1_irq_current:
    mov x0, #'I'
    bl uart10Putc
    mov x0, #'C'
    bl uart10Putc
    mov x0, #' '
    bl uart10Putc
    mrs x0, currentel
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    b .

el1_fiq_current:
    mov x0, #'F'
    bl uart10Putc
    mov x0, #'C'
    bl uart10Putc
    mov x0, #' '
    bl uart10Putc
    mrs x0, currentel
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc
    b .

el1_serror_current:
    mov x0, #'E'
    bl uart10Putc
    mov x0, #'C'
    bl uart10Putc
    mov x0, #':'
    bl uart10Putc
    mov x0, #'\r'
    bl uart10Putc
    mov x0, #'\n'
    bl uart10Putc

    b .
