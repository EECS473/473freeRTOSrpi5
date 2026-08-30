#include "gpio.h"

#include "rp1.h"

static uint32_t s_interruptPins;

static int validHeaderPin(GPIO_pin_t pin)
{
    return (unsigned)pin < 28u;
}

int gpio_pin_init(GPIO_pin_t pin,
                  GPIO_function_t pin_func,
                  GPIO_PULLx_t pin_pull)
{
    if (!validHeaderPin(pin))
    {
        return -1;
    }

    if (pin_func == OUT)
    {
        rp1GpioInitOutput((unsigned)pin);
        return 0;
    }

    if (pin_func != IN)
    {
        return -2;
    }

    uint32_t pull = RP1_PAD_PULL_NONE;

    if (pin_pull == GPIO_PIN_PULL_UP)
    {
        pull = RP1_PAD_PULL_UP;
    }
    else if (pin_pull == GPIO_PIN_PULL_DOWN)
    {
        pull = RP1_PAD_PULL_DOWN;
    }
    else if (pin_pull != GPIO_PIN_PULL_NON)
    {
        return -3;
    }

    rp1GpioInitInput((unsigned)pin, pull);
    return 0;
}

void gpio_pin_set(GPIO_pin_t pin, GPIO_set_clear_t value)
{
    rp1GpioWrite((unsigned)pin, value != GPIO_PIN_CLEAR);
}

void gpio_pin_toggle(GPIO_pin_t pin)
{
    rp1GpioWrite((unsigned)pin, !rp1GpioRead((unsigned)pin));
}

int gpio_pin_read(GPIO_pin_t pin)
{
    if (!validHeaderPin(pin))
    {
        return -1;
    }

    return rp1GpioRead((unsigned)pin);
}

int gpio_pin_isr_init(GPIO_pin_t pin, GPIO_event_t event_type)
{
    if (!validHeaderPin(pin))
    {
        return -1;
    }

    uint32_t event;

    switch (event_type)
    {
        case GPREN:
        case GPAREN:
            event = RP1_GPIO_IRQ_RISING;
            break;
        case GPFEN:
        case GPAFEN:
            event = RP1_GPIO_IRQ_FALLING;
            break;
        case GPHEN:
            event = RP1_GPIO_IRQ_HIGH;
            break;
        case GPLEN:
            event = RP1_GPIO_IRQ_LOW;
            break;
        default:
            return -2;
    }

    rp1GpioConfigureInterrupt((unsigned)pin, event);
    s_interruptPins |= 1u << (unsigned)pin;
    return 0;
}

void clear_isr_gpio21(void)
{
    if (rp1GpioInterruptPending(21u))
    {
        rp1GpioClearInterrupt(21u);
    }
}

uint32_t gpioCompatibilityInterruptPins(void)
{
    return s_interruptPins;
}
