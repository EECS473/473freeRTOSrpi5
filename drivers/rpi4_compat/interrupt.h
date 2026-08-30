#ifndef RPI4_COMPAT_INTERRUPT_H
#define RPI4_COMPAT_INTERRUPT_H

#include <stdint.h>

#define IRQ_GPIO0 (96u + 49u)
#define IRQ_GPIO1 (96u + 50u)
#define IRQ_GPIO2 (96u + 51u)
#define IRQ_GPIO3 (96u + 52u)

typedef void (*INTERRUPT_HANDLER)(void);

int isr_register(uint32_t intno,
                 uint32_t pri,
                 uint32_t cpumask,
                 INTERRUPT_HANDLER fn);
void eoi_notify(uint32_t val);
void wait_gic_init(void);

void compatibilityInterruptDispatch(void);

#endif
