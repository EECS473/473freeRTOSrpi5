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

    mrs x0, CNTHCTL_EL2
    orr x0, x0, #(1 << 0) | (1 << 1)
    msr CNTHCTL_EL2, x0

    mov x0, #0
    msr HCR_EL2, x0

    msr MDCR_EL2, xzr

    mrs x0, CNTHCTL_EL2
    bl uart10PrintHex64
    mov x0, #0x0d
    bl uart10Putc
    mov x0, #0x0a
    bl uart10Putc

    bl print_registers_el2

    eret


.section .text
.align 3
.global next
next:
    adrp x0, vector_el1
    add x0, x0, :lo12:vector_el1
    msr vbar_el1, x0
    isb

    b sysinit

sysinit:
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

    b el2_sync_lower_default
    .space 0x80-4
    b el2_irq_lower_default
    .space 0x80-4
    b el2_fiq_lower_default
    .space 0x80-4
    b el2_serror_lower_default
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
    stp x0, x1, [sp, #-16]!
    mov x0, #0
    bl uart10PrintHex64
    ldp x0, x1, [sp], #16
    b .

el2_fiq_current:
    b el2_irq_current

el2_serror_current:
    stp x0, x1, [sp, #-16]!
    mrs x0, esr_el2
    bl uart10PrintHex64
    ldp x0, x1, [sp], #16
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

el2_sync_lower_default:
    b el2_sync_lower

el2_irq_lower_default:
    b el2_irq_lower

el2_fiq_lower_default:
    b el2_fiq_lower

el2_serror_lower_default:
    b el2_serror_lower


.align 11
.global vector_el1
vector_el1:
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
    b .

el1_irq_sp0:
    b .

el1_fiq_sp0:
    b .

el1_serror_sp0:
    b .

el1_sync_current:
    b .

el1_irq_current:
    bl freertos_tick_handler
    /* SPSR_EL1, ELR_EL1 holds PSTATE and returing address of EL1 itself */
    eret 
    
el1_fiq_current:
    b .

el1_serror_current:
    b .
