#include "interrupt.h"

#include "gpio.h"
#include "rp1.h"

static INTERRUPT_HANDLER s_gpioHandler;

int isr_register(uint32_t intno,
                 uint32_t pri,
                 uint32_t cpumask,
                 INTERRUPT_HANDLER fn)
{
    if ((intno != IRQ_GPIO0) || (fn == 0))
    {
        return -1;
    }

    if ((pri > 0xffu) || (cpumask > 0xffu))
    {
        return -2;
    }

    int result = rp1InterruptTransportInit();
    if (result != 0)
    {
        return result;
    }

    s_gpioHandler = fn;

    uint32_t pins = gpioCompatibilityInterruptPins();
    for (unsigned pin = 0u; pin < 28u; ++pin)
    {
        if ((pins & (1u << pin)) != 0u)
        {
            rp1GpioClearInterrupt(pin);
            rp1GpioEnablePcieInterrupt(pin);
        }
    }

    return 0;
}

void compatibilityInterruptDispatch(void)
{
    if (s_gpioHandler != 0)
    {
        s_gpioHandler();
    }

    rp1AcknowledgeIoBank0();
}

void eoi_notify(uint32_t val)
{
    (void)val;
}

void wait_gic_init(void)
{
}
