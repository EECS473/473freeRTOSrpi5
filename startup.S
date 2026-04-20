.section .text.start, "ax", @progbits

.global _start
_start:
    mrs x19, mpidr_el1
    and x19, x19, #0xff

    mrs x20, currentel
    cmp x20, #(3 << 2)
    b.ne at_el2

    ldr x0, =g_cntfrq_el0
    ldr x0, [x0]
    msr cntfrq_el0, x0

    mov x0, #0x5b1
    msr scr_el3, x0

    msr cptr_el3, xzr
    isb

    ldr x0, =__stack_el2_end
    ldr x1, =__per_cpu_stack_el2_size
    mul x2, x19, x1
    sub x0, x0, x2
    msr sp_el2, x0


    adr x0, at_el2
    msr elr_el3, x0
    mov x0, #0x3c9
    msr spsr_el3, x0
    isb
    eret

at_el2:
    cbnz x19, 10f
    mrs x0, cntfrq_el0
    cbz x10, 10f
    adrp x1, g_cntfrq_el0
    add x1, x1, :lo12:g_cntfrq_el0
    str x0, [x1]
    dsb sy
10:
    ldr x0, =__stack_el2_end
    ldr x1, =__per_cpu_stack_el2_size
    mul x2, x19, x1
    sub x0, x0, x2
    mov sp, x0

    adrp x0, vector_el2
    add x0, x0, :lo12:vector_el2
    msr vbar_el2, x0
    isb

    ldr x0, =__stack_end
    ldr x1, =__per_cpu_stack_el2_size
    mul x2, x19, x1
    sub x0, x0, x2
    msr sp_el1, x0

    ldr x0, =el1_smp_selftest
    msr elr_el2, x0

    mov x0, #0x3c5
    msr spsr_el2, x0
    isb

    mrs x0, cnthctl_el2
    orr x0, x0, #(1 << 0) | (1 << 1)
    msr cnthctl_el2, x0

    mov x0, #0
    msr hcr_el2, x0
    msr mdcr_el2, xzr
    isb

    mov x0, x19
    eret


.align 3

.global el1_smp_selftest
el1_smp_selftest:
    mov x19, x0
    adrp x0, _freertos_vector_table
    add x0, x0, :lo12:_freertos_vector_table
    msr vbar_el1, x0
    isb

    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    isb

    cbnz x19, wait_turn

    ldr x0, =__bss_start
    ldr x1, =__bss_end
1:
    cmp x0, x1
    b.hs 2f
    str xzr, [x0], #8
    b 1b
2:
wait_turn:
    adrp x0, g_print_turn
    add x0, x0, :lo12:g_print_turn
    ldr w1, [x0]
    cmp w1, w19
    b.eq do_log
    wfe
    b wait_turn

do_log:
    mov x0, #'C'
    bl uart10Putc
    mov x0, #'P'
    bl uart10Putc
    mov x0, #'U'
    bl uart10Putc
    add x0, x19, #'0'
    bl uart10Putc
    mov x0, #' '
    bl uart10Putc
    mov x0, #'@'
    bl uart10Putc
    mov x0, #' '
    bl uart10Putc
    mov x0, #'E'
    bl uart10Putc
    mov x0, #'L'
    bl uart10Putc
    mrs x0, currentel
    lsr x0, x0, #2
    and x0, x0, #3
    add x0, x0, #'0'
    bl uart10Putc
    mov x0, #'\r'
    bl uart10Putc
    mov x0, #'\n'
    bl uart10Putc


    cbnz x19, bump_turn
    ldr x0, =_start
    mov x1, #0x100
    str x0, [x1]
    mov x0, #1
    mov x1, #0x108
    str x0, [x1]
    mov x1, #0x110
    str x0, [x1]
    mov x1, #0x118
    str x0, [x1]
    dsb sy
    sev

bump_turn:
    adrp x0, g_print_turn
    add x0, x0, :lo12:g_print_turn
    ldr w1, [x0]
    add w1, w1, #1
    str w1, [x0]
    dsb sy
    sev

halt:
    wfi
    b halt

.align 3

.global next
next:
    adrp x0, _freertos_vector_table
    add x0, x0, :lo12:_freertos_vector_table
    msr vbar_el1, x0
    isb

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
    b .


.align 3

.global secondary_entry
secondary_entry:

    adrp x1, _freertos_vector_table
    add x1, x1, :lo12:_freertos_vector_table
    msr vbar_el1, x1
    isb

    mrs x1, cpacr_el1
    orr x1, x1, #(3 << 20)
    msr cpacr_el1, x1
    isb

    b .
    
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
    b FreeRTOS_SWI_Handler

el1_irq_sp0:
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

.section .data
.align 3
.global g_cntfrq_el0
g_cntfrq_el0:
    .dword 54000000

.align 2
.global g_print_turn
g_print_turn:
    .word 0