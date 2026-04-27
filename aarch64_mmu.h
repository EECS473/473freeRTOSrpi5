#ifndef AARCH64_MMU_H
#define AARCH64_MMU_H

/*
 * EL1 MMU bring-up for BCM2712 (Pi 5). See aarch64_mmu.c for rationale.
 *
 * Required so that AArch64 LDAXR/STXR / LSE atomic instructions used by
 * SMP critical sections work. With MMU off, all RAM is Device memory
 * and those instructions take a Synchronous External Abort.
 *
 * Split of responsibilities:
 *   - C side (aarch64_mmu.c): build the shared L1 translation table.
 *     Just writes to a 4 KiB array; safe to run with MMU/cache off.
 *   - ASM side (startup.S, mmu_enable_el1):
 *     program MAIR/TCR/TTBR0, invalidate I-cache & TLB, then flip
 *     SCTLR_EL1.M|I (NOT D-cache - see startup.S for why).
 *     This is done in pure assembly so there is no function-call
 *     prologue/epilogue and therefore no stack access right around the
 *     "MMU just turned on" boundary. That avoids the classic
 *     mismatched-memory-attributes hang where the compiler-generated
 *     stp/ldp in the C function's prologue/epilogue would be issued
 *     non-cacheable on the way in and cacheable on the way out.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* L1 page table (4 KiB, 4 KiB-aligned), populated by CPU0 only.
 * Defined in aarch64_mmu.c. Exposed so startup.S can take its address. */
extern uint64_t g_aarch64_l1_table[ 512 ];

/* CPU0 only. Builds the shared L1 translation table in BSS. Pure C, no
 * MSR/MRS, can be called with MMU+cache off. */
void aarch64_mmu_build_table_cpu0( void );

#ifdef __cplusplus
}
#endif

#endif /* AARCH64_MMU_H */