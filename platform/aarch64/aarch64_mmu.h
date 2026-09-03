#ifndef AARCH64_MMU_H
#define AARCH64_MMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern uint64_t g_aarch64_l1_table[ 512 ];
void aarch64_mmu_build_table_cpu0( void );

#ifdef __cplusplus
}
#endif

#endif
