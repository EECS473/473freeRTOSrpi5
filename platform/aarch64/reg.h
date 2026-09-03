#ifndef REG_H_
#define REG_H_

#include <stdint.h>

#define REG32(BASE, OFFSET) (*(volatile uint32_t*)((BASE) + (OFFSET)))
#define REG64(BASE, OFFSET) (*(volatile uint64_t*)((BASE) + (OFFSET)))

#define MMIO_READ(TYPE, ADDR) (*(volatile TYPE*)(ADDR))
#define MMIO_WRITE(TYPE, ADDR, VAL) (*(volatile TYPE*)(ADDR) = (VAL))

#define MMIO_READ8(ADDR) MMIO_READ(uint8_t, ADDR)
#define MMIO_READ16(ADDR) MMIO_READ(uint16_t, ADDR)
#define MMIO_READ32(ADDR) MMIO_READ(uint32_t, ADDR)
#define MMIO_READ64(ADDR) MMIO_READ(uint64_t, ADDR)

#define MMIO_WRITE8(ADDR, VAL) MMIO_WRITE(uint8_t, ADDR, VAL)
#define MMIO_WRITE16(ADDR, VAL) MMIO_WRITE(uint16_t, ADDR, VAL)
#define MMIO_WRITE32(ADDR, VAL) MMIO_WRITE(uint32_t, ADDR, VAL)
#define MMIO_WRITE64(ADDR, VAL) MMIO_WRITE(uint64_t, ADDR, VAL)


#define SYSREG_READ(REG)                                                                                               \
    ({                                                                                                                 \
        uint64_t val;                                                                                                  \
        __asm__ volatile("mrs %0, " #REG : "=r"(val));                                                                 \
        val;                                                                                                           \
    })

#define SYSREG_WRITE(REG, VAL)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        uint64_t val = (uint64_t)(VAL);                                                                                \
        __asm__ volatile("msr " #REG ", %x0" : : "rZ"(val));                                                           \
    } while (0)

#endif
