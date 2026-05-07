/*
 * Raspberry Pi 5（BCM2712）SMP FreeRTOS 启动代码
 *
 * 当前阶段：主核接入 FreeRTOS、从核暂挂起
 *   - 四核先经 EL3(仅从核)/EL2 -> EL1 的 on_el1；这里在分岔前由**各核**清
 *     .bss（并发写 0 幂等），避免从核比 CPU0 早跑 C 代码时读到未初始化的
 *     ullPortSchedulerRunning 等变量。然后 CPU0 bl main；从核走 secondary。
 *   - 各核在进入 main / secondary_el1 之前先开 MMU+Cache（identity map，
 *     RAM 段 Normal Cacheable Inner-Shareable，外设段 Device），不然
 *     Cortex-A76 的 LDXR/STXR 与 LSE 原子指令对 Device 内存会触发外部异
 *     常（ESR DFSC=0x10），FreeRTOS SMP 关键区一进就挂。
 *   - CPU1..3 的 mailbox 释放由 main.c 的 bcm2712SmpBringupSecondaryCpus()。
 *
 * 关键实现要点（Pi 5 踩坑沉淀）：
 *   - BCM2712 的 core id 存在 MPIDR_EL1.Aff1（bits [15:8]），Aff0 恒为 0。
 *     与 Pi 3/4 不同。参见 TF-A plat_rpi5_calc_core_pos。
 *   - Pi 5 的 armstub8-2712.bin 是"假 BL31"：不提供 PSCI，从核启动用
 *     TF-A mailbox 布局（0x100 entry，0x108 起每核 8B hold flag，GO=1），
 *     但 BL33 要自己写。
 *   - 从核被 mailbox 释放后仍在 EL3，必须做完整的 EL3 prologue。
 *   - 从核 EL2 关键寄存器（CPTR_EL2 / CNTVOFF_EL2 / SCTLR_EL2）armstub
 *     没有替它初始化，需要在 EL3 阶段由我们写。
 *   - VideoCore firmware 已 init UART10，**不要**再调 uart10Init。
 *
 * 恢复/参考完整版：startup.S.full。
 */

    .section .text.start

    .global _start
_start:
    /* 进门屏蔽所有异常，防御 armstub 遗留状态。 */
    msr     DAIFSet, #0xf

    /* x19 = core id。BCM2712 在 MPIDR_EL1.Aff1，不是 Aff0。 */
    mrs     x19, mpidr_el1
    lsr     x19, x19, #8
    and     x19, x19, #0xff

    /* CurrentEL 的 [3:2] 字段编码 EL：EL3 = 3<<2 = 0xC。 */
    mrs     x20, CurrentEL
    cmp     x20, #(3 << 2)
    b.ne    on_el2

    /* ------------------------------------------------------------
     * EL3 -> EL2 下沉（仅从核 CPU1..3 走这里）
     * ------------------------------------------------------------ */

    /*
     * cntfrq_el0在ARMv8-A上是EL3-write only, pi5 firmware只在CPU0上设置过它
     * 为54MHz，CPU1..3被armstub的mailbox释放后落到这里时，这个值是不固定值
     */
    ldr     w0, =54000000
    msr     cntfrq_el0, x0
    isb

    msr     cptr_el3, xzr                   /* 不拦截下位 EL 的 FP/SIMD */
    isb

    mov     x0, #0x5b1                      /* NS|RES1|SMD|HCE|RW */
    msr     scr_el3, x0
    isb

    ldr     x0, =0x107FFF9080
    mvn     w1, wzr
    str     w1, [x0]

    ldr     x0, =0x107FFF9100
    mov     w1, #(1 << 30)
    str     w1, [x0]

    ldr     x0, =0x107FFF9400
    ldr     w1, =0xE0E0E0E0
    str     w1, [x0]
    str     w1, [x0, #4]
    str     w1, [x0, #8]
    str     w1, [x0, #12]

    ldr     x0, =0x107FFFA000
    mov     w1, #0x07
    str     w1, [x0]

    ldr     x0, =0x107FFFA004
    mov     w1, #0xF0
    str     w1, [x0]

    dsb     sy
    isb

    ldr     x0, =__stack_el2_end
    ldr     x1, =__per_cpu_stack_el2_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    msr     sp_el2, x0

    /* EL2 关键寄存器预配（从核这条路径 armstub 没走过）。 */
    msr     cptr_el2, xzr                   /* TFP=0：允许 EL1/0 FP/SIMD */
    msr     cntvoff_el2, xzr                /* 虚拟计数器对齐物理 */
    ldr     x0, =0x30c50830                 /* SCTLR_EL2 reset-safe */
    msr     sctlr_el2, x0
    isb

    /* eret 到 on_el2 @ EL2h, DAIF 全屏蔽。 */
    adr     x0, on_el2
    msr     elr_el3, x0
    mov     x0, #0x3c9                      /* EL2h + DAIF=1111 */
    msr     spsr_el3, x0
    isb
    eret


    /* ------------------------------------------------------------
     * on_el2: EL2 -> EL1 下沉（4 核都走这里）
     * ------------------------------------------------------------ */
on_el2:
    /* SP_EL2（CPU0 第一次设置；从核重写一遍保持对称）。 */
    ldr     x0, =__stack_el2_end
    ldr     x1, =__per_cpu_stack_el2_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    mov     sp, x0

    /* 安装 EL2 向量表：万一 EL2 阶段发生异常（sync/serror 等），
     * 跳到 vector_el2 里的 handler 打印信息而不是乱飞。 */
    adrp    x0, vector_el2
    add     x0, x0, :lo12:vector_el2
    msr     vbar_el2, x0
    isb

    /* SP_EL1 = __stack_end - core_id * __per_cpu_stack_size */
    ldr     x0, =__stack_end
    ldr     x1, =__per_cpu_stack_size
    mul     x2, x19, x1
    sub     x0, x0, x2
    msr     sp_el1, x0

    /* 允许 EL1 访问 physical counter/timer */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #(1 << 0) | (1 << 1)    /* EL1PCTEN | EL1PCEN */
    msr     cnthctl_el2, x0

    /* HCR_EL2 = 0（A76 上 RW 为 RES1 自动为 1，EL1 保持 AArch64）
     * MDCR_EL2 = 0 清 Debug trap */
    msr     hcr_el2, xzr
    msr     mdcr_el2, xzr
    isb

    /* eret 到 on_el1 @ EL1h, DAIF 全屏蔽。 */
    adr     x0, on_el1
    msr     elr_el2, x0
    mov     x0, #0x3c5                      /* EL1h + DAIF=1111 */
    msr     spsr_el2, x0
    isb
    eret


    /* ------------------------------------------------------------
     * on_el1: 所有核在 EL1 的汇合点
     *   x19 = core id（eret 不改通用寄存器）
     * ------------------------------------------------------------ */
on_el1:
    /* 安装真正的 FreeRTOS EL1 向量表：SWI/IRQ 分派给 FreeRTOS 的
     * FreeRTOS_SWI_Handler / FreeRTOS_IRQ_Handler。 */
    adrp    x0, _freertos_vector_table
    add     x0, x0, :lo12:_freertos_vector_table
    msr     vbar_el1, x0
    isb

    /* 打开 EL1/0 的 FP/SIMD 访问。 */
    mrs     x0, cpacr_el1
    orr     x0, x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    /* 清 .bss：必须在从核走 bl vPortStartSchedulerOnSecondaryCore 等任意 C
     * 之前完成。仅 CPU0 清会有竞态：从核在 ullPortSchedulerRunning 等被
     * CPU0 写到 0 之前就读到随机值，可能错误退出等待并破坏内存。多核对同
     * 一区间并发 str xzr 是幂等且可接受的。 */
    cbnz    x19, clear_bss_done
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
clear_bss:
    cmp     x0, x1
    b.hs    clear_bss_done
    str     xzr, [x0], #8
    b       clear_bss
clear_bss_done:

    /* MMU 必须先开：FreeRTOS SMP 关键区会用 LDXR/STXR (或 LSE swpa)，这
     * 些指令在 Cortex-A76 上要求目标内存是 Normal 类型；MMU 关闭时所有
     * 内存被当 Device，会触发 Synchronous External Abort。
     *
     * 流程：
     *   1) CPU0 调 C 函数 aarch64_mmu_build_table_cpu0 在 .bss 里把 L1
     *      表填好（512 个 1 GB block，前 16 GB Normal IS Cacheable，其
     *      余 Device-nGnRnE）。从核不参与建表。
     *   2) 所有核走 mmu_enable_el1 这段**纯汇编**：写好
     *      MAIR/TCR/TTBR0，做 IC/TLBI，最后翻 SCTLR.M|I。
     *
     * 这一步**故意不走 C**：否则编译器在函数 prologue 的
     * stp x29,x30,[sp,#-16]! 与 epilogue 的 ldp+ret 会跨"MMU 刚打开"
     * 这条边界——前者以 non-cacheable 写 stack，后者以 cacheable 读
     * stack，A76 上这种 mismatched memory attributes 会让核当场吊死。
     *
     * 必须开 D-cache (SCTLR_EL1.C)：A76 的 LL/SC monitor 与 LSE 原子在
     * Normal Inner-Shareable Non-Cacheable 内存上是 implementation-
     * defined，实测会以 Synchronous External Abort 返回（ESR 0x96000410，
     * DFSC=0x10）。开 D-cache 之前必须先做 D-cache invalidate by set/
     * way，否则 boot ROM/firmware 残留在 cache 里的脏行会立刻和 DRAM
     * 起冲突。AArch64 没有"一条指令清光所有 D-cache"，只能循环 dc isw。 */
    cbnz    x19, mmu_enable_el1
    bl      aarch64_mmu_build_table_cpu0

    /* ---------------- 所有核：开 MMU + I-cache + D-cache ---------------- */
mmu_enable_el1:
    /* 1) D-cache invalidate by set/way（针对所有本核可见的 cache 层级）。
     *    Cortex-A76 + DSU 上 LoC 通常是 3，包含 L1D + L2 + L3。代码用
     *    CLIDR/CCSIDR 自适应不依赖具体几级。
     *
     *    寄存器约定（不与外面冲突）：
     *      x0  = clidr_el1
     *      x3  = LoC * 2 (终止条件)
     *      x9  = working "way" counter
     *      x10 = current level << 1 (= csselr value)
     *      x11 = composed dc isw operand
     *      x1, x2, x4-x7 工作寄存器
     */
    mrs     x0, clidr_el1
    and     x3, x0, #0x7000000           /* CLIDR.LoC = bits[26:24]   */
    lsr     x3, x3, #23                  /* LoC * 2                   */
    cbz     x3, mmu_dcache_inv_done
    mov     x10, #0                      /* level 0, csselr = 0       */

mmu_dcache_inv_level:
    add     x2, x10, x10, lsr #1         /* x2 = level * 3            */
    lsr     x1, x0, x2                   /* x1 = clidr >> (level * 3) */
    and     x1, x1, #7                   /* Ctype for this level      */
    cmp     x1, #2                       /* 2,3,4 都有 D-cache        */
    b.lt    mmu_dcache_inv_next

    msr     csselr_el1, x10              /* 选这个 cache              */
    isb
    mrs     x1, ccsidr_el1               /* 读它的几何形状            */

    and     x2, x1, #7                   /* x2 = log2(line)-4         */
    add     x2, x2, #4                   /* log2(line size in bytes)  */
    mov     x4, #0x3ff
    and     x4, x4, x1, lsr #3           /* ways - 1                  */
    clz     w5, w4                       /* way 字段在 dc isw 里的左移 */
    mov     x7, #0x7fff
    and     x7, x7, x1, lsr #13          /* sets - 1                  */

mmu_dcache_inv_set:
    mov     x9, x4
mmu_dcache_inv_way:
    lsl     x6, x9, x5                   /* way << way_shift          */
    orr     x11, x10, x6                 /* level | way               */
    lsl     x6, x7, x2                   /* set << line_shift         */
    orr     x11, x11, x6                 /* full dc isw operand       */
    dc      isw, x11                     /* invalidate this set/way   */
    subs    x9, x9, #1
    b.ge    mmu_dcache_inv_way
    subs    x7, x7, #1
    b.ge    mmu_dcache_inv_set

mmu_dcache_inv_next:
    add     x10, x10, #2                 /* 下一层 cache              */
    cmp     x3, x10
    b.gt    mmu_dcache_inv_level

mmu_dcache_inv_done:
    msr     csselr_el1, xzr
    dsb     sy
    isb

    /* 2) MAIR_EL1：[0]=Device-nGnRnE (0x00), [1]=Normal WB-WA (0xFF) */
    ldr     x0, =mmu_mair_el1_value
    ldr     x0, [x0]
    msr     mair_el1, x0

    /* 3) TCR_EL1：T0SZ=25 (39-bit VA), 4K granule, IS, IPS=40-bit, EPD1=1 */
    ldr     x0, =mmu_tcr_el1_value
    ldr     x0, [x0]
    msr     tcr_el1, x0

    /* 4) TTBR0_EL1 = &g_aarch64_l1_table */
    ldr     x0, =g_aarch64_l1_table
    msr     ttbr0_el1, x0

    /* 5) 清 I-cache、TLB（D-cache 已经在上面 invalidate 过了） */
    ic      iallu
    tlbi    vmalle1
    dsb     ish
    isb

    /* 6) 翻 SCTLR_EL1.M | .C | .I，三个一起开。
     *    M：MMU；C：D-cache；I：I-cache。 */
    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 0)            /* M  - MMU enable      */
    orr     x0, x0, #(1 << 2)            /* C  - D-cache enable  */
    orr     x0, x0, #(1 << 12)           /* I  - I-cache enable  */
    msr     sctlr_el1, x0
    isb

    /* 从核跳 secondary_el1；CPU0 进 main。 */
    cbnz    x19, secondary_el1

    bl      main

    /* main 正常不返回。如果返回了就挂起防止乱跑。 */
    b       halt


    /* ------------------------------------------------------------
     * secondary_el1: 从核 CPU1..3 的 EL1 入口
     *
     * 精简版 on_el1 已经装好 vbar_el1 和 cpacr_el1.FPEN，这里只需要把
     * core id 传给 port 层即可。
     *
     * 约定：FreeRTOS port.c 里的 vPortStartSchedulerOnSecondaryCore(
     *     uint32_t ulCoreID) 会：
     *   1) mask IRQ，等 CPU0 把 ullPortSchedulerRunning 置 1；
     *   2) 本核 GIC CPU interface 初始化（vPortSetupGicCpuInterface）；
     *   3) vPortRestoreTaskContext() 进入本核的第一个任务，永不返回。
     * ------------------------------------------------------------ */
secondary_el1:
    mov     w0, w19                         /* uint32_t core id */
    bl      vPortStartSchedulerOnSecondaryCore
    b       halt                            /* 正常不返回，防御性兜底 */


halt:
    wfi
    b       halt


    /* ============================================================
     * 通用代码段：向量表 + 异常 handler
     * ============================================================ */
    .section .text

    /* ---------- EL2 向量表（2KB 对齐，16 槽，每槽 0x80 B）---------- */
    .align  11
    .global vector_el2
vector_el2:
    b       el2_sync_sp0
    .space  0x80 - 4
    b       el2_irq_sp0
    .space  0x80 - 4
    b       el2_fiq_sp0
    .space  0x80 - 4
    b       el2_serror_sp0
    .space  0x80 - 4

    b       el2_sync_current
    .space  0x80 - 4
    b       el2_irq_current
    .space  0x80 - 4
    b       el2_fiq_current
    .space  0x80 - 4
    b       el2_serror_current
    .space  0x80 - 4

    b       el2_sync_lower
    .space  0x80 - 4
    b       el2_irq_lower
    .space  0x80 - 4
    b       el2_fiq_lower
    .space  0x80 - 4
    b       el2_serror_lower
    .space  0x80 - 4

    b       el2_sync_lower_aarch32
    .space  0x80 - 4
    b       el2_irq_lower_aarch32
    .space  0x80 - 4
    b       el2_fiq_lower_aarch32
    .space  0x80 - 4
    b       el2_serror_lower_aarch32
    .space  0x80 - 4

    /* EL2 handler（所有落到这里的都打印寄存器信息+死循环，方便调试）。 */
el2_sync_sp0:
    b       el2_sync_current

el2_irq_sp0:
    b       el2_irq_current

el2_fiq_sp0:
    b       el2_fiq_current

el2_serror_sp0:
    b       el2_serror_current

el2_sync_current:
    stp     x0, x1, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    mrs     x0, esr_el2
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    mrs     x0, elr_el2
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    mrs     x0, far_el2
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    mrs     x0, spsr_el2
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    b       .

el2_irq_current:
    b       .

el2_fiq_current:
    b       .

el2_serror_current:
    b       .

el2_sync_lower:
    stp     x0, x1, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!
    mrs     x0, esr_el2
    bl      uart10PrintHex64
    b       .

el2_irq_lower:
    b       .

el2_fiq_lower:
    b       .

el2_serror_lower:
    b       .

el2_sync_lower_aarch32:
    b       .

el2_irq_lower_aarch32:
    b       .

el2_fiq_lower_aarch32:
    b       .

el2_serror_lower_aarch32:
    b       .


    /* ---------- EL1 向量表（2KB 对齐，16 槽）---------- */
    .align  11
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

    /*
     * FreeRTOS 约定：使用 SPSel=0（SP_EL0）时的 sync 异常就是 svc（系统
     * 调用），FreeRTOS 用 svc 做任务切换；IRQ 走 FreeRTOS_IRQ_Handler。
     * 这两个 handler 定义在 FreeRTOS-Kernel 的 portASM.S 里。
     */
el1_sync_sp0:
    b       FreeRTOS_SWI_Handler

el1_irq_sp0:
    b       FreeRTOS_IRQ_Handler

    /* 其它 EL1 异常打印信息 + 死循环，方便调试时定位。 */
el1_fiq_sp0:
    mov     x0, #'F'
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc
    b       .

el1_serror_sp0:
    mov     x0, #'E'
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #'\n'
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

    mrs     x0, esr_el1
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    mrs     x0, elr_el1
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc

    mrs     x0, far_el1
    bl      uart10PrintHex64
    mov     x0, #'\n'
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
    mov     x0, #'\n'
    bl      uart10Putc
    b       FreeRTOS_IRQ_Handler

el1_fiq_current:
    mov     x0, #'F'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #' '
    bl      uart10Putc
    mrs     x0, currentel
    bl      uart10PrintHex64
    mov     x0, #'\n'
    bl      uart10Putc
    b       .

el1_serror_current:
    mov     x0, #'E'
    bl      uart10Putc
    mov     x0, #'C'
    bl      uart10Putc
    mov     x0, #':'
    bl      uart10Putc
    mov     x0, #'\n'
    bl      uart10Putc
    b       .