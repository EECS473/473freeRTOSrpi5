/*
 * Minimal AArch64 EL1 MMU bring-up for BCM2712 (Raspberry Pi 5).
 *
 * Why this exists
 * ---------------
 * Cortex-A76 (and Armv8-A in general) requires the target memory of
 *   - Load-/Store-Exclusive (LDXR/STXR/LDAXR/STLXR), and
 *   - LSE atomic instructions (LDADD, SWP, CAS, ...)
 * to be **Normal memory**. Issuing any of those against memory marked
 * Device generates a Synchronous External Abort (DFSC=0x10, CM=1).
 *
 * With the MMU disabled (SCTLR_EL1.M = 0) every access is treated as
 * Device-nGnRnE. That is harmless for plain loads/stores from a single
 * core, which is why a single-core build appears to work fine. It is
 * fatal as soon as the SMP scheduler comes up: tasks.c critical sections
 * call vPortRecursiveLock() which uses __atomic_exchange_n(uint32) on
 * ulPortLockGate[]. GCC lowers that to either an LSE swpa (if available)
 * or an ldaxr/stxr loop via libgcc's outline atomics
 * (__aarch64_swp4_acq). Both fault the moment the scheduler enters its
 * first critical section, with ELR pointing inside libgcc's lse.S.
 *
 * The only architecturally correct fix is to turn the MMU on with the
 * RAM region mapped as Normal Inner-Shareable Cacheable. This file does
 * exactly that, with the simplest layout that works for Pi 5:
 *
 *   - Single L1 table, 512 entries x 1 GB blocks = 512 GB of VA space
 *   - 39-bit VA (T0SZ = 25), 4 KB granule, TTBR0 only (TTBR1 disabled)
 *   - Identity map: VA == PA
 *   - First 16 GB (covers any Pi 5 SKU's DRAM) -> Normal WB-WA, IS
 *   - Remaining 496 GB -> Device-nGnRnE (covers all BCM2712 MMIO incl.
 *     GIC at 0x107FFF9000 and the armstub mailbox at 0x000000d8 which
 *     happens to live inside the Normal region; that is fine for our
 *     uses because we DSB-SY after writing it and never expect cache
 *     coherency between the armstub and BL33 for those bytes).
 *
 * Bring-up sequence
 * -----------------
 *   CPU0:                  aarch64_mmu_build_table_cpu0();
 *                          [startup.S:mmu_enable_el1]
 *   CPU1..3:               [startup.S:mmu_enable_el1]
 *
 * The build step writes to the L1 table while caches are still off, so
 * the writes hit DRAM directly. A DSB SY orders those writes before any
 * core enables its MMU.
 *
 * The enable step lives in startup.S as pure assembly. It is critical
 * that "open the MMU" happens without any function-call prologue or
 * epilogue around it, otherwise the stack writes go out non-cacheable
 * and the matching reads come back cacheable, which on Cortex-A76
 * triggers mismatched-memory-attributes behaviour and tends to hang the
 * core right after the SCTLR_EL1 write. See startup.S:mmu_enable_el1.
 */

#include <stdint.h>

#include "aarch64_mmu.h"

/* ------------------------------------------------------------------ */
/* MAIR_EL1 attribute encodings                                       */
/* ------------------------------------------------------------------ */
#define MAIR_ATTR_DEVICE_NGNRNE     0x00ULL
/* Normal: Outer Write-Back Non-transient RW-Allocate,
 *         Inner Write-Back Non-transient RW-Allocate.
 *   bits[7:4] = 0b1111 (Outer WB RW-Alloc non-transient)
 *   bits[3:0] = 0b1111 (Inner WB RW-Alloc non-transient)
 */
#define MAIR_ATTR_NORMAL_WB         0xFFULL

#define MAIR_IDX_DEVICE             0U
#define MAIR_IDX_NORMAL             1U

/* MAIR_EL1: pack both attributes at their indices. */
#define MAIR_EL1_VALUE                                 \
    ( ( MAIR_ATTR_NORMAL_WB << ( MAIR_IDX_NORMAL * 8 ) ) | \
      ( MAIR_ATTR_DEVICE_NGNRNE << ( MAIR_IDX_DEVICE * 8 ) ) )

/* ------------------------------------------------------------------ */
/* L1 block descriptor bit fields (stage-1, 1 GB blocks at level 1)   */
/* ------------------------------------------------------------------ */
#define DESC_VALID                  ( 1ULL << 0 )
/* For a level-1 entry: bits[1:0] = 01 -> 1 GB block descriptor. */
#define DESC_BLOCK                  ( 0ULL << 1 )

#define DESC_ATTRIDX( idx )         ( ( ( uint64_t ) ( idx ) & 0x7ULL ) << 2 )
#define DESC_NS                     ( 1ULL << 5 )            /* non-secure */
#define DESC_AP_RW_EL1              ( 0ULL << 6 )            /* AP[2:1] = 00 */
#define DESC_SH_NON                 ( 0ULL << 8 )            /* SH = 00 */
#define DESC_SH_INNER               ( 3ULL << 8 )            /* SH = 11 */
#define DESC_AF                     ( 1ULL << 10 )           /* Access flag */
#define DESC_NG                     ( 0ULL << 11 )           /* global */

/* 1 GB block, identity map: output address == VA == PA. */
#define BLOCK_NORMAL_IS( pa )                         \
    ( ( ( uint64_t )( pa ) ) | DESC_AF | DESC_SH_INNER |  \
      DESC_AP_RW_EL1 | DESC_ATTRIDX( MAIR_IDX_NORMAL ) |  \
      DESC_BLOCK | DESC_VALID )

#define BLOCK_DEVICE( pa )                            \
    ( ( ( uint64_t )( pa ) ) | DESC_AF | DESC_SH_NON |    \
      DESC_AP_RW_EL1 | DESC_ATTRIDX( MAIR_IDX_DEVICE ) |  \
      DESC_BLOCK | DESC_VALID )

/* ------------------------------------------------------------------ */
/* Shared L1 translation table                                        */
/*                                                                    */
/* Has to be 4 KiB aligned (TG0=4K). Lives in .bss so it is zero-init  */
/* at boot and visible to every core once their MMU is on. The symbol */
/* is referenced from startup.S (mmu_enable_el1), so it is a global   */
/* (no `static`).                                                     */
/* ------------------------------------------------------------------ */
#define L1_NUM_ENTRIES              512U

uint64_t g_aarch64_l1_table[ L1_NUM_ENTRIES ]
    __attribute__( ( aligned( 4096 ) ) );

/* "How many of the low 1 GB blocks are RAM (Normal Cacheable)?"
 * 16 GB is enough to cover every Pi 5 SKU (max 16 GB DRAM today).
 */
#define RAM_NORMAL_GB               16U

void aarch64_mmu_build_table_cpu0( void )
{
    for( uint32_t idx = 0U; idx < L1_NUM_ENTRIES; ++idx )
    {
        uint64_t pa = ( ( uint64_t ) idx ) << 30;            /* idx * 1 GB */

        if( idx < RAM_NORMAL_GB )
        {
            g_aarch64_l1_table[ idx ] = BLOCK_NORMAL_IS( pa );
        }
        else
        {
            g_aarch64_l1_table[ idx ] = BLOCK_DEVICE( pa );
        }
    }

    /* Make sure the table is in DRAM before any core enables its MMU.
     * Caches are still off here  this is just a memory barrier, but
     * the DSB is required so the table walker (once enabled) sees the
     * final values. */
    __asm__ volatile ( "dsb sy" ::: "memory" );
}

/* MAIR_EL1 / TCR_EL1 expected values, exported for startup.S. Defining
 * them as `const` (not `extern`) puts the value in .rodata so the asm
 * `ldr xN, =mmu_mair_el1_value` PC-relative load works correctly even
 * with MMU off. */
const uint64_t mmu_mair_el1_value = MAIR_EL1_VALUE;

#define TCR_EL1_T0SZ( n )           ( ( uint64_t ) ( n ) & 0x3FULL )
#define TCR_EL1_IRGN0_WBWA          ( 1ULL << 8 )
#define TCR_EL1_ORGN0_WBWA          ( 1ULL << 10 )
#define TCR_EL1_SH0_INNER           ( 3ULL << 12 )
#define TCR_EL1_TG0_4K              ( 0ULL << 14 )
#define TCR_EL1_EPD1                ( 1ULL << 23 )
#define TCR_EL1_IPS_40BIT           ( 2ULL << 32 )

const uint64_t mmu_tcr_el1_value =
    ( TCR_EL1_T0SZ( 25 ) | TCR_EL1_IRGN0_WBWA | TCR_EL1_ORGN0_WBWA |
      TCR_EL1_SH0_INNER | TCR_EL1_TG0_4K | TCR_EL1_EPD1 | TCR_EL1_IPS_40BIT );


