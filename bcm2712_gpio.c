#include "bcm2712_gpio.h"

void ledOff(void)
{
    uint32_t gio_data = GIO_DATA;
    gio_data &= ~0x200; /* Set bit 9 to 0, refs: bcm2712-rpi-5-b.dts */
    GIO_DATA = gio_data;
}

void ledOn(void)
{
    uint32_t gio_data = GIO_DATA;
    gio_data |= 0x200; /* Set bit 9 to 1, refs: bcm2712-rpi-5-b.dts */
    GIO_DATA = gio_data;
}
