#ifndef RP1_H
#define RP1_H

#include <stdint.h>

/*
 * ---------------------------------------------------------------------------
 * BCM2712 / PCIe2
 * ---------------------------------------------------------------------------
 *
 * Raspberry Pi 5 RP1 is connected through PCIe controller 2.
 */
#define BCM2712_PCIE2_BASE              UINT64_C(0x1000120000)

#define PCIE_MISC_PCIE_STATUS           0x4068u
#define PCIE_STATUS_PHYLINKUP           (1u << 4)
#define PCIE_STATUS_DL_ACTIVE           (1u << 5)

#define PCIE_EXT_CFG_DATA               0x8000u
#define PCIE_EXT_CFG_INDEX              0x9000u

/*
 * PCI configuration space.
 */
#define PCI_VENDOR_DEVICE               0x00u
#define PCI_COMMAND                     0x04u
#define PCI_STATUS                      0x06u
#define PCI_BAR0                        0x10u
#define PCI_BAR1                        0x14u
#define PCI_BAR2                        0x18u
#define PCI_CAPABILITY_LIST             0x34u

#define PCI_COMMAND_MEMORY              (1u << 1)
#define PCI_COMMAND_MASTER              (1u << 2)
#define PCI_COMMAND_INTX_DISABLE        (1u << 10)

#define PCI_STATUS_CAP_LIST             (1u << 4)

#define PCI_CAP_ID_MSIX                 0x11u

#define PCI_MSIX_FLAGS                  0x02u
#define PCI_MSIX_TABLE                  0x04u

#define PCI_MSIX_FLAGS_QSIZE            0x07ffu
#define PCI_MSIX_FLAGS_MASKALL          0x4000u
#define PCI_MSIX_FLAGS_ENABLE           0x8000u

#define PCI_MSIX_TABLE_BIR              0x7u
#define PCI_MSIX_TABLE_OFFSET           (~0x7u)

/*
 * RP1 PCI identity.
 */
#define RP1_VENDOR_ID                   0x1de4u
#define RP1_DEVICE_ID                   0x0001u

/*
 * ---------------------------------------------------------------------------
 * RP1 CPU-visible BAR1 peripheral aperture
 * ---------------------------------------------------------------------------
 *
 * With the Pi 5 firmware configuration used by this project:
 *
 *     enable_rp1_uart=1
 *     pciex4_reset=0
 *
 * the boot firmware leaves RP1 BAR1 mapped into the BCM2712 CPU address
 * space at 0x1c00000000.  The target's boot UART confirms this because RP1
 * UART0 (BAR1 offset 0x30000) is working at 0x1c00030000.
 *
 * The PCI bus address assigned to BAR1 is firmware-dependent (on the current
 * target it is in the firmware's 2-GiB PCI window), so rp1.c discovers that
 * bus address at runtime and uses BAR1 as the translation anchor for BAR0/1/2.
 */
#define RP1_CPU_BASE                    UINT64_C(0x1c00000000)

#define RP1_SYSINFO_BASE                (RP1_CPU_BASE + UINT64_C(0x000000))
#define RP1_IO_BANK0_BASE               (RP1_CPU_BASE + UINT64_C(0x0d0000))
#define RP1_SYS_RIO0_BASE               (RP1_CPU_BASE + UINT64_C(0x0e0000))
#define RP1_PADS_BANK0_BASE             (RP1_CPU_BASE + UINT64_C(0x0f0000))
#define RP1_PCIE_APBS_BASE              (RP1_CPU_BASE + UINT64_C(0x108000))

/*
 * RP1 peripheral atomic aliases.
 */
#define RP1_RW_OFFSET                   0x0000u
#define RP1_XOR_OFFSET                  0x1000u
#define RP1_SET_OFFSET                  0x2000u
#define RP1_CLR_OFFSET                  0x3000u

/*
 * ---------------------------------------------------------------------------
 * RP1 IO_BANK0
 * ---------------------------------------------------------------------------
 */

#define RP1_GPIO_STATUS(pin)            (0x0000u + ((pin) * 8u))
#define RP1_GPIO_CTRL(pin)              (0x0004u + ((pin) * 8u))

#define RP1_GPIO_PCIE_INTE              0x011cu
#define RP1_GPIO_PCIE_INTS              0x0124u

#define RP1_GPIO_FUNCSEL_MASK           0x0000001fu

/* GPIO is function-select 5 on RP1. */
#define RP1_GPIO_FUNC_GPIO              5u

#define RP1_GPIO_OUTOVER_MASK           0x00003000u
#define RP1_GPIO_OEOVER_MASK            0x0000c000u
#define RP1_GPIO_INOVER_MASK            0x00030000u
#define RP1_GPIO_IRQOVER_MASK           0xc0000000u

/*
 * Per-GPIO interrupt enables.
 *
 * Bits 20-23: raw events
 * Bits 24-27: filtered events
 */
#define RP1_GPIO_IRQ_FALLING            (1u << 20)
#define RP1_GPIO_IRQ_RISING             (1u << 21)
#define RP1_GPIO_IRQ_LOW                (1u << 22)
#define RP1_GPIO_IRQ_HIGH               (1u << 23)

#define RP1_GPIO_IRQ_F_FALLING          (1u << 24)
#define RP1_GPIO_IRQ_F_RISING           (1u << 25)
#define RP1_GPIO_IRQ_F_LOW              (1u << 26)
#define RP1_GPIO_IRQ_F_HIGH             (1u << 27)

#define RP1_GPIO_IRQ_MASK               (0xffu << 20)

/*
 * Writing 1 here clears a stored edge event.
 */
#define RP1_GPIO_IRQRESET               (1u << 28)

/*
 * ---------------------------------------------------------------------------
 * RP1 SYS_RIO0
 * ---------------------------------------------------------------------------
 */

#define RP1_RIO_OUT                     0x00u
#define RP1_RIO_OE                      0x04u
#define RP1_RIO_IN                      0x08u

/*
 * ---------------------------------------------------------------------------
 * RP1 PADS_BANK0
 * ---------------------------------------------------------------------------
 *
 * GPIO0 starts at offset 4.
 */
#define RP1_PAD_GPIO(pin)               (0x04u + ((pin) * 4u))

#define RP1_PAD_PULL_MASK               0x0000000cu

#define RP1_PAD_PULL_NONE               0x00000000u
#define RP1_PAD_PULL_DOWN               0x00000004u
#define RP1_PAD_PULL_UP                 0x00000008u

#define RP1_PAD_INPUT_ENABLE            0x00000040u
#define RP1_PAD_OUTPUT_DISABLE          0x00000080u

/*
 * ---------------------------------------------------------------------------
 * RP1 local interrupt controller
 * ---------------------------------------------------------------------------
 *
 * Important:
 * These aliases are specific to the RP1 PCIe controller interrupt block.
 * They are NOT the ordinary +0x2000/+0x3000 peripheral aliases.
 */
#define RP1_PCIE_REG_SET                0x0800u
#define RP1_PCIE_REG_CLR                0x0c00u

#define RP1_MSIX_CFG(irq)               (0x008u + ((irq) * 4u))

#define RP1_MSIX_CFG_ENABLE             (1u << 0)
#define RP1_MSIX_CFG_TEST               (1u << 1)
#define RP1_MSIX_CFG_IACK               (1u << 2)
#define RP1_MSIX_CFG_IACK_EN            (1u << 3)

#define RP1_INTSTATL                    0x0108u
#define RP1_INTSTATH                    0x010cu

/*
 * IO_BANK0 is RP1 interrupt source / MSI-X vector 0.
 */
#define RP1_INT_IO_BANK0                0u

/*
 * ---------------------------------------------------------------------------
 * BCM2712 MIP0
 * ---------------------------------------------------------------------------
 *
 * MSI-X writes sent to PCIe address ff_ffff_f000 arrive here.
 */
#define BCM2712_MIP0_BASE               UINT64_C(0x1000130000)

#define MIP_INT_RAISE                   0x00u
#define MIP_INT_CLEAR                   0x10u
#define MIP_INT_CFGL_HOST               0x20u
#define MIP_INT_CFGH_HOST               0x30u
#define MIP_INT_MASKL_HOST              0x40u
#define MIP_INT_MASKH_HOST              0x50u

/*
 * RP1 MSI-X message address for MIP0.
 */
#define MIP0_MSIX_MESSAGE_ADDRESS       UINT64_C(0x000000fffffff000)

/*
 * MIP0 maps MSI vector 0 to GIC SPI 128.
 *
 * GIC interrupt IDs:
 *      SGI = 0-15
 *      PPI = 16-31
 *      SPI = 32+
 *
 * Therefore SPI 128 => INTID 160.
 */
#define RP1_GPIO_GIC_SPI                128u
#define RP1_GPIO_GIC_INTID              (32u + RP1_GPIO_GIC_SPI)

/*
 * We use interrupt priority 10 because this ISR calls FreeRTOS FromISR APIs.
 *
 * configMAX_API_CALL_INTERRUPT_PRIORITY is 9 in this project.
 */
#define RP1_GPIO_GIC_PRIORITY           10u

/*
 * ---------------------------------------------------------------------------
 * MMIO helpers
 * ---------------------------------------------------------------------------
 */

static inline uint32_t rp1MmioRead32(uint64_t address)
{
    uint32_t value = *(volatile uint32_t *)(uintptr_t)address;

    __asm__ volatile(
        "dmb oshld"
        :
        :
        : "memory");

    return value;
}

static inline void rp1MmioWrite32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;

    __asm__ volatile(
        "dsb oshst"
        :
        :
        : "memory");
}

static inline uint8_t rp1MmioRead8(uint64_t address)
{
    uint8_t value = *(volatile uint8_t *)(uintptr_t)address;

    __asm__ volatile(
        "dmb oshld"
        :
        :
        : "memory");

    return value;
}

static inline void rp1MmioWrite8(uint64_t address, uint8_t value)
{
    *(volatile uint8_t *)(uintptr_t)address = value;

    __asm__ volatile(
        "dsb oshst"
        :
        :
        : "memory");
}

/*
 * ---------------------------------------------------------------------------
 * Public driver API
 * ---------------------------------------------------------------------------
 */

int rp1InterruptTransportInit(void);

uint32_t rp1GetChipId(void);

void rp1GpioInitOutput(unsigned pin);
void rp1GpioInitInput(unsigned pin, uint32_t pull);
void rp1GpioInitInputPullDown(unsigned pin);

void rp1GpioWrite(unsigned pin, int high);
int rp1GpioRead(unsigned pin);

void rp1GpioConfigureRisingInterrupt(unsigned pin);
void rp1GpioConfigureInterrupt(unsigned pin, uint32_t events);
void rp1GpioEnablePcieInterrupt(unsigned pin);
void rp1GpioDisablePcieInterrupt(unsigned pin);

int rp1GpioInterruptPending(unsigned pin);
void rp1GpioClearInterrupt(unsigned pin);

void rp1AcknowledgeIoBank0(void);

#endif
