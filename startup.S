    .section .text.start, "ax", @progbits
    
    .global _start
_start:
    msr     daifset, #0xf

    mrs     x19, mpidr_el1
    lsr     x19, x19, #8
    and     x19, x19, #0xff

    mrs     x20, currentel
    cmp     x20, #(3 << 2)
    b.ne    .Lat_el2

    ldr     x0, =__stack_el3_end
    ldr     x1, =__per_cpu_stack_el3_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    mov     sp, x0

    msr     cptr_el3, xzr
    isb

    mov     x0, #0x5b1
    msr     scr_el3, x0
    isb

    ldr     x0, =__stack_el2_end
    ldr     x1, =__per_cpu_stack_el2_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    msr     sp_el2, x0

    msr     cptr_el2, xzr
    msr     cntvoff_el2, xzr
    ldr     x0, =0x30c50830
    msr     sctlr_el2, x0
    isb

    adr     x0, .Lat_el2
    msr     elr_el3, x0
    mov     x0, #0x3c9
    msr     spsr_el3, x0
    isb
    eret

.Lat_el2:
    ldr     x0, =__stack_el2_end
    ldr     x1, =__per_cpu_stack_el2_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    mov     sp, x0

    ldr     x0, =__stack_el1_end
    ldr     x1, =__per_cpu_stack_el1_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    msr     sp_el1, x0


    mrs     x0, cnthctl_el2
    orr     x0, x0, #(1 << 0) | (1 << 1) /* EL1PCTEN | EL1PCEN */
    msr     cnthctl_el2, x0

    msr     hcr_el2, xzr
    msr     mdcr_el2, xzr
    isb

    adr     x0, .Lat_el1
    msr     elr_el2, x0
    mov     x0, #0x3c5
    msr     spsr_el2, x0
    isb
    eret

.Lat_el1:
    adrp    x0, _freertos_vector_table
    add     x0, x0, :lo12:_freertos_vector_table
    msr     vbar_el1, x0
    isb

    mrs     x0, cpacr_el1
    orr     x0, x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    cbnz    x19, secondary_el1

    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
1:  cmp     x0, x1
    b.hs    2f
    str     xzr, [x0], #8
    b       1b
2:
    ldr     x0, =_start
    mov     x1, #0x100
    str     x0, [x1]
    mov     x0, #1
    mov     x1, #0x110
    str     x0, [x1]
    mov     x1, #0x118
    str     x0, [x1]
    mov     x1, #0x120
    str     x0, [x1]
    dsb     sy
    sev

    b       primary_el1

secondary_el1:
    b       halt

primary_el1:
    bl      main
    b       halt

halt:
    wfi
    b       halt

    .section .text

    .align 11
    .global _freertos_vector_table
_freertos_vector_table:
    b       el1_sync_sp0
    .space  0x80 - 4
    b       el1_irq_sp0
    .space  0x80 - 4
    b       el1_fiq_sp0
    .space  0x80 - 4
    b       el1_serror_sp0
    .space  0x80 - 4

    b       el1_sync_current
    .space  0x80 - 4
    b       el1_irq_current
    .space  0x80 - 4
    b       el1_fiq_current
    .space  0x80 - 4
    b       el1_serror_current
    .space  0x80 - 4

el1_sync_sp0:
    b       FreeRTOS_SWI_Handler

el1_irq_sp0:
    b       FreeRTOS_IRQ_Handler

el1_fiq_sp0:
    mov     x0, #'F'
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc
    b       .

el1_serror_sp0:
    mov     x0, #'E'
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc
    b       .

el1_sync_current:
    mov     x0, #'S'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #':'
    bl      uart10Putc
    mov     x0, #'\n'
    bl      uart10Putc
    mov     x0, #'\r'
    bl      uart10Putc

    mrs     x0, esr_el1
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc

    mrs     x0, elr_el1
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc


    mrs     x0, far_el1
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc

    b       .

el1_irq_current:
    mov     x0, #'I'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #' '
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc

    b       .

el1_fiq_current:
    mov     x0, #'F'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #' '
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #0x0d
    bl      uart10Putc
    mov     x0, #0x0a
    bl      uart10Putc

    b       .

el1_serror_current:
    mov     x0, #'E'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #':'
    bl      uart10Putc
    mov     x0, #'\r'
    bl      uart10Putc
    mov     x0, #'\n'
    bl      uart10Putc

    b       .