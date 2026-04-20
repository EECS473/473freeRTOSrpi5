#ifndef PLATFORM_DEF_H_
#define PLATFORM_DEF_H

/* Mailbox to control the secondary cores. All secondary cores are held in a
 * wait loop in cold boot. To release them perform the following steps (plus
 * any additional barriers that may be needed):
 *
 *     uint64_t *entrypoint = (uint64_t *)PLAT_RPI3_TM_ENTRYPOINT;
 *     *entrypoint = ADDRESS_TO_JUMP_TO;
 *
 *     uint64_t *mbox_entry = (uint64_t *)PLAT_RPI3_TM_HOLD_BASE;
 *     mbox_entry[cpu_id] = PLAT_RPI3_TM_HOLD_STATE_GO;
 *
 *     sev();
 *
 * The secure entry point to be used on warm reset by all CPUs.
 */
#define PLAT_RPI3_TM_ENTRYPOINT     0x100
#define PLAT_RPI3_TM_ENTRYPOINT_SIZE    8ULL

/* Hold entries for each CPU. */
#define PLAT_RPI3_TM_HOLD_BASE      (PLAT_RPI3_TM_ENTRYPOINT + PLAT_RPI3_TM_ENTRYPOINT_SIZE)
#define PLAT_RPI3_TM_HOLD_ENTRY_SIZE    8ULL
#define PLAT_RPI3_TM_HOLD_SIZE      (PLAT_RPI3_TM_HOLD_ENTRY_SIZE * PLATFORM_CORE_COUNT)

#define PLAT_RPI3_TRUSTED_MAILBOX_SIZE  (PLAT_RPI3_TM_ENTRYPOINT_SIZE + PLAT_RPI3_TM_HOLD_SIZE)

#define PLAT_RPI3_TM_HOLD_STATE_WAIT    0ULL
#define PLAT_RPI3_TM_HOLD_STATE_GO  1ULL
#define PLAT_RPI3_TM_HOLD_STATE_BSP_OFF 2ULL


#endif
