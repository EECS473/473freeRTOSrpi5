#include "rp1.h"
#include "bcm2712.h"
#include "rp1_uart0.h"

#include "FreeRTOS.h"

#include <stddef.h>
#include <stdint.h>

/*
 * ============================================================================
 * RP1 support for Raspberry Pi 5
 * ============================================================================
 *
 * This file provides:
 *
 *   - PCI configuration-space access to RP1
 *   - RP1 BAR / MSI-X setup
 *   - BCM2712 MIP0 interrupt setup
 *   - BCM2712 GIC setup for RP1 interrupt vector 0
 *   - RP1 GPIO input/output
 *   - RP1 GPIO rising-edge interrupt support
 *
 * ============================================================================
 */


/*
 * ============================================================================
 * GICv2 Distributor registers
 * ============================================================================
 *
 * GICD_BASE is provided by bcm2712.h:
 *
 *     BCM2712_PER_BASE + 0x7FFF9000
 *
 * which evaluates to:
 *
 *     0x107FFF9000
 */

#define GICD_IGROUPR_BASE       (GICD_BASE + 0x0080u)
#define GICD_ISENABLER_BASE     (GICD_BASE + 0x0100u)
#define GICD_ICENABLER_BASE     (GICD_BASE + 0x0180u)
#define GICD_ICPENDR_BASE       (GICD_BASE + 0x0280u)
#define GICD_IPRIORITYR_BASE    (GICD_BASE + 0x0400u)
#define GICD_ITARGETSR_BASE     (GICD_BASE + 0x0800u)
#define GICD_ICFGR_BASE         (GICD_BASE + 0x0c00u)


/*
 * ============================================================================
 * PCI helpers
 * ============================================================================
 */

/*
 * Normal Pi 5 firmware enumeration places RP1 BAR0 here in PCI bus
 * address space.
 */
#define RP1_EXPECTED_BAR0_BUS_ADDRESS \
    UINT64_C(0x00410000)

/* PCI-to-PCI bridge bus-number register in the BCM2712 root complex. */
#define PCIE_BRIDGE_BUS_NUMBERS     0x18u

/*
 * BCM2712 PCIe root-complex inbound-window registers.
 *
 * On BCM2712, the Linux brcmstb PCIe driver treats the controller as the
 * BCM7712-style core.  Inbound RC BAR3 is used for the special 4-KiB
 * MIP0 MSI-X window:
 *
 *     PCIe ff_ffff_f000 -> CPU 10_00130000
 */
#define PCIE_MISC_RC_BAR1_CONFIG_LO             0x402cu
#define PCIE_MISC_UBUS_BAR1_CONFIG_REMAP        0x40acu
#define PCIE_MISC_UBUS_BAR_ACCESS_EN             (1u << 0)
#define PCIE_IBAR_SIZE_4K_CODE                   0x1cu
#define PCIE_MIP0_INBOUND_BAR                    3u



/*
 * RP1 normally appears on the secondary bus behind BCM2712 PCIe2.
 *
 * We discover the actual secondary-bus number during initialization.
 */
static uint8_t s_rp1Bus = 1u;


/*
 * ============================================================================
 * PCI configuration-space access
 * ============================================================================
 */

/*
 * BCM2712 external configuration cycles use an ECAM-style index:
 *
 *     bits 27:20 = bus
 *     bits 19:15 = device
 *     bits 14:12 = function
 *
 * RP1 is expected at:
 *
 *     device   = 0
 *     function = 0
 */

static uint32_t pciEcamIndex(uint8_t bus,
                             uint8_t device,
                             uint8_t function)
{
    return ((uint32_t)bus << 20) |
           ((uint32_t)device << 15) |
           ((uint32_t)function << 12);
}


static uint64_t pciConfigAddress(uint8_t bus,
                                 uint8_t device,
                                 uint8_t function,
                                 uint16_t offset)
{
    uint32_t index =
        pciEcamIndex(bus,
                     device,
                     function);

    rp1MmioWrite32(
        BCM2712_PCIE2_BASE +
        PCIE_EXT_CFG_INDEX,
        index);

    return BCM2712_PCIE2_BASE +
           PCIE_EXT_CFG_DATA +
           ((uint32_t)offset & 0xffcu);
}


static uint32_t pciRead32(uint8_t bus,
                          uint8_t device,
                          uint8_t function,
                          uint16_t offset)
{
    uint64_t address =
        pciConfigAddress(bus,
                         device,
                         function,
                         offset);

    return rp1MmioRead32(address);
}


static uint16_t pciRead16(uint8_t bus,
                          uint8_t device,
                          uint8_t function,
                          uint16_t offset)
{
    uint32_t value =
        pciRead32(
            bus,
            device,
            function,
            (uint16_t)(offset & ~3u));

    unsigned shift =
        (unsigned)(offset & 2u) * 8u;

    return (uint16_t)(
        (value >> shift) &
        0xffffu);
}


static uint8_t pciRead8(uint8_t bus,
                        uint8_t device,
                        uint8_t function,
                        uint16_t offset)
{
    uint32_t value =
        pciRead32(
            bus,
            device,
            function,
            (uint16_t)(offset & ~3u));

    unsigned shift =
        (unsigned)(offset & 3u) * 8u;

    return (uint8_t)(
        (value >> shift) &
        0xffu);
}


static void pciWrite32(uint8_t bus,
                       uint8_t device,
                       uint8_t function,
                       uint16_t offset,
                       uint32_t value)
{
    uint64_t address =
        pciConfigAddress(bus,
                         device,
                         function,
                         offset);

    rp1MmioWrite32(
        address,
        value);
}


static void pciWrite16(uint8_t bus,
                       uint8_t device,
                       uint8_t function,
                       uint16_t offset,
                       uint16_t value)
{
    uint16_t aligned =
        (uint16_t)(offset & ~3u);

    uint32_t oldValue =
        pciRead32(
            bus,
            device,
            function,
            aligned);

    unsigned shift =
        (unsigned)(offset & 2u) * 8u;

    uint32_t mask =
        0xffffu << shift;

    uint32_t newValue =
        (oldValue & ~mask) |
        ((uint32_t)value << shift);

    pciWrite32(
        bus,
        device,
        function,
        aligned,
        newValue);
}


/*
 * ============================================================================
 * PCI BAR helpers
 * ============================================================================
 */

static uint64_t pciReadBarAddress(unsigned bar)
{
    if (bar > 5u)
    {
        return UINT64_MAX;
    }

    uint16_t offset =
        (uint16_t)(
            PCI_BAR0 +
            (bar * 4u));

    uint32_t low =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            offset);

    /*
     * Bit zero set means I/O-space BAR.
     *
     * RP1 uses memory BARs, so reject I/O BARs.
     */
    if ((low & 1u) != 0u)
    {
        return UINT64_MAX;
    }

    unsigned type =
        (low >> 1) & 3u;

    uint64_t address =
        (uint64_t)(
            low &
            ~0x0fu);

    /*
     * PCI memory BAR type:
     *
     *     00 = 32-bit
     *     10 = 64-bit
     */
    if (type == 2u)
    {
        if (bar == 5u)
        {
            return UINT64_MAX;
        }

        uint32_t high =
            pciRead32(
                s_rp1Bus,
                0u,
                0u,
                (uint16_t)(
                    offset + 4u));

        address |=
            ((uint64_t)high << 32);
    }
    else if (type != 0u)
    {
        return UINT64_MAX;
    }

    return address;
}


/*
 * Translate a PCIe bus address into the CPU-visible BCM2712 address.
 *
 * PCIe2 provides two useful outbound mappings:
 *
 *     PCI bus 0x00000000
 *          ->
 *     CPU 0x1f00000000
 *
 * and a larger 64-bit prefetchable region.
 */

static uint64_t s_rp1Bar1BusAddress = UINT64_MAX;

static uint64_t pciBusToCpu(uint64_t busAddress)
{
    if ((s_rp1Bar1BusAddress == UINT64_MAX) ||
        (busAddress < s_rp1Bar1BusAddress))
    {
        return UINT64_MAX;
    }

    uint64_t offset =
        busAddress - s_rp1Bar1BusAddress;

    /*
     * The firmware places RP1 BAR0/BAR1/BAR2 together in the same
     * outbound PCIe window.  BAR1 is the RP1 peripheral aperture and is
     * known to be CPU-visible at RP1_CPU_BASE.
     */
    if (offset >= UINT64_C(0x01000000))
    {
        return UINT64_MAX;
    }

    return RP1_CPU_BASE + offset;
}


/*
 * ============================================================================
 * MSI-X capability lookup
 * ============================================================================
 */

static uint16_t findMsixCapability(void)
{
    uint16_t status =
        pciRead16(
            s_rp1Bus,
            0u,
            0u,
            PCI_STATUS);

    /*
     * Device must advertise a PCI capability list.
     */
    if ((status &
         PCI_STATUS_CAP_LIST) == 0u)
    {
        return 0u;
    }

    uint8_t capability =
        pciRead8(
            s_rp1Bus,
            0u,
            0u,
            PCI_CAPABILITY_LIST);

    /*
     * Capability pointers are DWORD aligned.
     */
    capability &= ~3u;

    /*
     * Protect against corrupt/malformed linked lists.
     */
    for (unsigned i = 0u;
         (i < 48u) &&
         (capability >= 0x40u);
         ++i)
    {
        uint8_t id =
            pciRead8(
                s_rp1Bus,
                0u,
                0u,
                capability);

        if (id ==
            PCI_CAP_ID_MSIX)
        {
            return capability;
        }

        uint8_t next =
            pciRead8(
                s_rp1Bus,
                0u,
                0u,
                (uint16_t)(
                    capability + 1u));

        next &= ~3u;

        if ((next == 0u) ||
            (next == capability))
        {
            break;
        }

        capability = next;
    }

    return 0u;
}


/*
 * ============================================================================
 * BCM2712 GIC configuration
 * ============================================================================
 */

static void gicConfigureRp1Vector(void)
{
    const unsigned intId =
        RP1_GPIO_GIC_INTID;

    const uint32_t bit =
        1u << (intId & 31u);


    /*
     * ------------------------------------------------------------------------
     * Disable interrupt while configuring it.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite32(
        GICD_ICENABLER_BASE +
            ((intId / 32u) * 4u),
        bit);


    /*
     * ------------------------------------------------------------------------
     * Put the SPI into Group 1.
     * ------------------------------------------------------------------------
     */

    uint64_t groupAddress =
        GICD_IGROUPR_BASE +
        ((intId / 32u) * 4u);

    uint32_t group =
        rp1MmioRead32(
            groupAddress);

    group |= bit;

    rp1MmioWrite32(
        groupAddress,
        group);


    /*
     * ------------------------------------------------------------------------
     * Configure the GIC interrupt as edge-triggered.
     * ------------------------------------------------------------------------
     *
     * Two configuration bits per interrupt:
     *
     *     00 = level
     *     10 = edge
     */

    uint64_t cfgAddress =
        GICD_ICFGR_BASE +
        ((intId / 16u) * 4u);

    unsigned cfgShift =
        (intId & 15u) * 2u;

    uint32_t cfg =
        rp1MmioRead32(
            cfgAddress);

    cfg &=
        ~(3u << cfgShift);

    cfg |=
        (2u << cfgShift);

    rp1MmioWrite32(
        cfgAddress,
        cfg);


    /*
     * ------------------------------------------------------------------------
     * Interrupt priority
     * ------------------------------------------------------------------------
     *
     * FreeRTOSConfig.h:
     *
     *     configUNIQUE_INTERRUPT_PRIORITIES = 16
     *     configMAX_API_CALL_INTERRUPT_PRIORITY = 9
     *
     * Our RP1 GPIO interrupt uses priority 10.
     *
     * Since numerically smaller values are more urgent, priority 10 is
     * permitted to call xSemaphoreGiveFromISR().
     */

    rp1MmioWrite8(
        GICD_IPRIORITYR_BASE +
        intId,
        (uint8_t)(
            RP1_GPIO_GIC_PRIORITY <<
            portPRIORITY_SHIFT));


    /*
     * ------------------------------------------------------------------------
     * Route interrupt to CPU0.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite8(
        GICD_ITARGETSR_BASE +
        intId,
        0x01u);


    /*
     * ------------------------------------------------------------------------
     * Clear stale pending state.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite32(
        GICD_ICPENDR_BASE +
            ((intId / 32u) * 4u),
        bit);


    __asm__ volatile(
        "dsb sy\n"
        "isb"
        :
        :
        : "memory");


    /*
     * ------------------------------------------------------------------------
     * Enable interrupt.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite32(
        GICD_ISENABLER_BASE +
            ((intId / 32u) * 4u),
        bit);
}


/*
 * ============================================================================
 * BCM2712 PCIe2 inbound MSI-X window for MIP0
 * ============================================================================
 */

static uint64_t pcieInboundBarConfigAddress(unsigned bar)
{
    return BCM2712_PCIE2_BASE +
           PCIE_MISC_RC_BAR1_CONFIG_LO +
           ((uint64_t)(bar - 1u) * UINT64_C(8));
}

static uint64_t pcieInboundBarRemapAddress(unsigned bar)
{
    return BCM2712_PCIE2_BASE +
           PCIE_MISC_UBUS_BAR1_CONFIG_REMAP +
           ((uint64_t)(bar - 1u) * UINT64_C(8));
}

static void pcie2ConfigureMip0InboundWindow(void)
{
    const uint64_t pciAddress =
        MIP0_MSIX_MESSAGE_ADDRESS;

    const uint64_t cpuAddress =
        BCM2712_MIP0_BASE;

    const uint64_t config =
        pcieInboundBarConfigAddress(PCIE_MIP0_INBOUND_BAR);

    const uint64_t remap =
        pcieInboundBarRemapAddress(PCIE_MIP0_INBOUND_BAR);

    /*
     * BCM2712/7712 inbound BAR format:
     *
     *   CONFIG_LO[4:0] = encoded size
     *
     * A 4-KiB window uses size code 0x1c.
     */
    uint32_t configLow =
        ((uint32_t)pciAddress & ~0x1fu) |
        PCIE_IBAR_SIZE_4K_CODE;

    rp1MmioWrite32(config + 0u, configLow);
    rp1MmioWrite32(config + 4u, (uint32_t)(pciAddress >> 32));

    /*
     * Map the accepted PCIe write onto the MIP0 MMIO block.
     */
    uint32_t remapLow =
        ((uint32_t)cpuAddress & ~0xfffu) |
        PCIE_MISC_UBUS_BAR_ACCESS_EN;

    rp1MmioWrite32(remap + 0u, remapLow);
    rp1MmioWrite32(remap + 4u, (uint32_t)(cpuAddress >> 32));

    __asm__ volatile(
        "dsb sy\n"
        "isb"
        :
        :
        : "memory");

    rp1Uart0Puts("[rp1] PCIe2 MIP0 inbound BAR3 cfg lo = 0x");
    rp1Uart0PrintHex32(rp1MmioRead32(config + 0u));
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] PCIe2 MIP0 inbound BAR3 cfg hi = 0x");
    rp1Uart0PrintHex32(rp1MmioRead32(config + 4u));
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] PCIe2 MIP0 inbound BAR3 remap lo = 0x");
    rp1Uart0PrintHex32(rp1MmioRead32(remap + 0u));
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] PCIe2 MIP0 inbound BAR3 remap hi = 0x");
    rp1Uart0PrintHex32(rp1MmioRead32(remap + 4u));
    rp1Uart0Puts("\n");
}


/*
 * ============================================================================
 * BCM2712 MIP0
 * ============================================================================
 */

static void mip0ConfigureVectorZero(void)
{
    const uint32_t bit =
        1u;


    /*
     * Configure MIP0 host vector zero as edge-triggered.
     */
    uint32_t cfg =
        rp1MmioRead32(
            BCM2712_MIP0_BASE +
            MIP_INT_CFGL_HOST);

    cfg |= bit;

    rp1MmioWrite32(
        BCM2712_MIP0_BASE +
            MIP_INT_CFGL_HOST,
        cfg);


    /*
     * Unmask vector zero.
     *
     * Zero means unmasked.
     */
    uint32_t mask =
        rp1MmioRead32(
            BCM2712_MIP0_BASE +
            MIP_INT_MASKL_HOST);

    mask &= ~bit;

    rp1MmioWrite32(
        BCM2712_MIP0_BASE +
            MIP_INT_MASKL_HOST,
        mask);


    /*
     * Clear stale pending state.
     */
    rp1MmioWrite32(
        BCM2712_MIP0_BASE +
            MIP_INT_CLEAR,
        bit);
}


/*
 * ============================================================================
 * RP1 MSI-X table
 * ============================================================================
 */

static int configureRp1MsixVectorZero(
    uint16_t msixCapability)
{
    uint16_t messageControl =
        pciRead16(
            s_rp1Bus,
            0u,
            0u,
            (uint16_t)(
                msixCapability +
                PCI_MSIX_FLAGS));


    unsigned vectorCount =
        (messageControl &
         PCI_MSIX_FLAGS_QSIZE) +
        1u;

    if (vectorCount < 1u)
    {
        return -1;
    }


    /*
     * ------------------------------------------------------------------------
     * Mask all MSI-X vectors while changing the table.
     * ------------------------------------------------------------------------
     */

    messageControl |=
        PCI_MSIX_FLAGS_MASKALL;

    pciWrite16(
        s_rp1Bus,
        0u,
        0u,
        (uint16_t)(
            msixCapability +
            PCI_MSIX_FLAGS),
        messageControl);


    /*
     * ------------------------------------------------------------------------
     * Locate the MSI-X table.
     * ------------------------------------------------------------------------
     */

    uint32_t table =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            (uint16_t)(
                msixCapability +
                PCI_MSIX_TABLE));

    unsigned bir =
        table &
        PCI_MSIX_TABLE_BIR;

    uint64_t tableOffset =
        (uint64_t)(
            table &
            PCI_MSIX_TABLE_OFFSET);

    if (bir > 5u)
    {
        return -2;
    }


    uint64_t barAddress =
        pciReadBarAddress(
            bir);

    if (barAddress ==
        UINT64_MAX)
    {
        return -3;
    }


    uint64_t tableCpuBase =
        pciBusToCpu(
            barAddress);

    if (tableCpuBase ==
        UINT64_MAX)
    {
        return -4;
    }


    /*
     * MSI-X table entries are 16 bytes each.
     *
     * We only need entry zero.
     */
    uint64_t entry =
        tableCpuBase +
        tableOffset;


    /*
     * ------------------------------------------------------------------------
     * Mask vector zero while programming it.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite32(
        entry + 0x0cu,
        1u);


    /*
     * ------------------------------------------------------------------------
     * Program MSI message address.
     * ------------------------------------------------------------------------
     *
     * BCM2712 MIP0 accepts MSI writes at:
     *
     *     0x000000fffffff000
     */

    rp1MmioWrite32(
        entry + 0x00u,
        (uint32_t)
            MIP0_MSIX_MESSAGE_ADDRESS);

    rp1MmioWrite32(
        entry + 0x04u,
        (uint32_t)(
            MIP0_MSIX_MESSAGE_ADDRESS >>
            32));


    /*
     * Message data zero selects MIP0 vector zero.
     */
    rp1MmioWrite32(
        entry + 0x08u,
        0u);


    __asm__ volatile(
        "dsb sy"
        :
        :
        : "memory");


    /*
     * ------------------------------------------------------------------------
     * Unmask vector zero.
     * ------------------------------------------------------------------------
     */

    rp1MmioWrite32(
        entry + 0x0cu,
        0u);


    /*
     * ------------------------------------------------------------------------
     * Enable MSI-X globally.
     * ------------------------------------------------------------------------
     */

    messageControl |=
        PCI_MSIX_FLAGS_ENABLE;

    messageControl &=
        ~PCI_MSIX_FLAGS_MASKALL;

    pciWrite16(
        s_rp1Bus,
        0u,
        0u,
        (uint16_t)(
            msixCapability +
            PCI_MSIX_FLAGS),
        messageControl);


    return 0;
}


/*
 * ============================================================================
 * RP1 local interrupt source
 * ============================================================================
 */

static void enableRp1IoBank0MsixSource(void)
{
    uint64_t cfgAddress =
        RP1_PCIE_APBS_BASE +
        RP1_PCIE_REG_SET +
        RP1_MSIX_CFG(
            RP1_INT_IO_BANK0);


    /*
     * IO_BANK0 is level-high at the RP1 local interrupt-controller level.
     *
     * IACK_EN tells RP1 to suppress additional MSI-X messages until the
     * software interrupt handler explicitly acknowledges the source.
     */
    rp1MmioWrite32(
        cfgAddress,
        RP1_MSIX_CFG_ENABLE |
        RP1_MSIX_CFG_IACK_EN |
        RP1_MSIX_CFG_IACK);
}


/*
 * ============================================================================
 * Public interrupt transport initialization
 * ============================================================================
 *
 * Return values:
 *
 *      0     success
 *
 *     -1     PCIe2 link is not active
 *     -2     PCIe2 secondary bus number is not configured
 *     -3     RP1 PCI device not found
 *     -4     RP1 BAR1 is not mapped at PCI bus address zero
 *     -5     RP1 MSI-X capability was not found
 *     -6     RP1 MSI-X table setup failed
 */

int rp1InterruptTransportInit(void)
{
    /*
     * ------------------------------------------------------------------------
     * Verify PCIe2 link.
     * ------------------------------------------------------------------------
     */

    uint32_t pcieStatus =
        rp1MmioRead32(
            BCM2712_PCIE2_BASE +
            PCIE_MISC_PCIE_STATUS);

    if ((pcieStatus &
         (PCIE_STATUS_PHYLINKUP |
          PCIE_STATUS_DL_ACTIVE)) !=
        (PCIE_STATUS_PHYLINKUP |
         PCIE_STATUS_DL_ACTIVE))
    {
        return -1;
    }


/*
 * ------------------------------------------------------------------------
 * Configure PCI bridge bus numbers.
 * ------------------------------------------------------------------------
 *
 * PCI bridge register 0x18:
 *
 *     bits  7:0   primary bus
 *     bits 15:8   secondary bus
 *     bits 23:16  subordinate bus
 *     bits 31:24  secondary latency timer
 *
 * The Pi firmware leaves the RP1 PCIe link running for us when
 * pciex4_reset=0 is used, but it may leave the bridge bus numbers
 * at zero.
 *
 * RP1 is the first and only endpoint immediately behind this root
 * bridge, so use:
 *
 *     primary     = 0
 *     secondary   = 1
 *     subordinate = 1
 */

uint32_t busNumbers =
    rp1MmioRead32(
        BCM2712_PCIE2_BASE +
        PCIE_BRIDGE_BUS_NUMBERS);

s_rp1Bus =
    (uint8_t)(
        (busNumbers >> 8) &
        0xffu);

if (s_rp1Bus == 0u)
{
    /*
     * Preserve the secondary-latency-timer byte but program
     * primary/secondary/subordinate bus numbers.
     *
     * Value of low 24 bits:
     *
     *     primary     = 0x00
     *     secondary   = 0x01
     *     subordinate = 0x01
     *
     * Therefore:
     *
     *     0x00010100
     */

    busNumbers =
        (busNumbers & 0xff000000u) |
        0x00010100u;

    rp1MmioWrite32(
        BCM2712_PCIE2_BASE +
        PCIE_BRIDGE_BUS_NUMBERS,
        busNumbers);

    /*
     * Make sure the bridge configuration write completes before
     * attempting downstream PCI configuration accesses.
     */
    __asm__ volatile(
        "dsb sy\n"
        "isb"
        :
        :
        : "memory");

    /*
     * Read it back.
     */
    busNumbers =
        rp1MmioRead32(
            BCM2712_PCIE2_BASE +
            PCIE_BRIDGE_BUS_NUMBERS);

    s_rp1Bus =
        (uint8_t)(
            (busNumbers >> 8) &
            0xffu);

    /*
     * If the secondary bus still did not become bus 1, something
     * more fundamental is wrong with the root-complex setup.
     */
    if (s_rp1Bus != 1u)
    {
        return -2;
    }
}

    /*
     * ------------------------------------------------------------------------
     * Confirm RP1 PCI identity.
     * ------------------------------------------------------------------------
     */

    uint32_t id =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            PCI_VENDOR_DEVICE);

    uint16_t vendor =
        (uint16_t)(
            id &
            0xffffu);

    uint16_t device =
        (uint16_t)(
            id >>
            16);

    if ((vendor !=
         RP1_VENDOR_ID) ||
        (device !=
         RP1_DEVICE_ID))
    {
        return -3;
    }


    /*
     * ------------------------------------------------------------------------
     * Inspect the RP1 PCI command register and BAR assignments.
     * ------------------------------------------------------------------------
     *
     * IMPORTANT:
     *
     * Do NOT disable PCI memory decoding while printing these diagnostics.
     * Our console is RP1 UART0, so disabling RP1 memory decoding would also
     * make the UART MMIO aperture disappear while we are trying to print.
     */

    uint16_t command =
        pciRead16(
            s_rp1Bus,
            0u,
            0u,
            PCI_COMMAND);

    uint32_t rawBar0 =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            PCI_BAR0);

    uint32_t rawBar1 =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            PCI_BAR1);

    uint32_t rawBar2 =
        pciRead32(
            s_rp1Bus,
            0u,
            0u,
            PCI_BAR2);

    uint64_t bar0 =
        pciReadBarAddress(
            0u);

    uint64_t bar1 =
        pciReadBarAddress(
            1u);

    if (bar1 == UINT64_MAX)
    {
        return -4;
    }

    /*
     * BAR1 is the runtime PCI->CPU translation anchor.
     */
    s_rp1Bar1BusAddress = bar1;

    uint64_t bar2 =
        pciReadBarAddress(
            2u);


    rp1Uart0Puts("\n[rp1] PCI command = 0x");
    rp1Uart0PrintHex16(command);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] raw BAR0 = 0x");
    rp1Uart0PrintHex32(rawBar0);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] raw BAR1 = 0x");
    rp1Uart0PrintHex32(rawBar1);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] raw BAR2 = 0x");
    rp1Uart0PrintHex32(rawBar2);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR0 PCI = 0x");
    rp1Uart0PrintHex64(bar0);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR1 PCI = 0x");
    rp1Uart0PrintHex64(bar1);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR2 PCI = 0x");
    rp1Uart0PrintHex64(bar2);
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR0 CPU = 0x");
    rp1Uart0PrintHex64(
        pciBusToCpu(bar0));
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR1 CPU = 0x");
    rp1Uart0PrintHex64(
        pciBusToCpu(bar1));
    rp1Uart0Puts("\n");

    rp1Uart0Puts("[rp1] BAR2 CPU = 0x");
    rp1Uart0PrintHex64(
        pciBusToCpu(bar2));
    rp1Uart0Puts("\n");


    /*
     * ------------------------------------------------------------------------
     * BAR0 contains the MSI-X table.
     * ------------------------------------------------------------------------
     *
     * Normal Linux enumeration commonly places BAR0 at PCI bus address
     * 0x00410000 when BAR1 occupies bus address zero.  If BAR0 has not been
     * assigned at all, assign that fallback value.
     */

    if (bar0 == 0u)
    {
        /*
         * BAR writes must be done with endpoint memory decoding disabled.
         * We only do that here, after all UART diagnostics are complete.
         */
        pciWrite16(
            s_rp1Bus,
            0u,
            0u,
            PCI_COMMAND,
            (uint16_t)(
                command &
                ~PCI_COMMAND_MEMORY));

        pciWrite32(
            s_rp1Bus,
            0u,
            0u,
            PCI_BAR0,
            (uint32_t)
                RP1_EXPECTED_BAR0_BUS_ADDRESS);
    }


    /*
     * ------------------------------------------------------------------------
     * Enable memory-space access and bus mastering.
     * ------------------------------------------------------------------------
     *
     * INTx is disabled because this driver uses MSI-X.
     */

    command |=
        PCI_COMMAND_MEMORY |
        PCI_COMMAND_MASTER |
        PCI_COMMAND_INTX_DISABLE;

    pciWrite16(
        s_rp1Bus,
        0u,
        0u,
        PCI_COMMAND,
        command);


    /*
     * ------------------------------------------------------------------------
     * Verify RP1 MMIO aperture.
     * ------------------------------------------------------------------------
     *
     * A successful read demonstrates that BAR1/MMIO access is working.
     */

    uint32_t chipId = rp1GetChipId();

    rp1Uart0Puts("[rp1] chip ID = 0x");
    rp1Uart0PrintHex32(chipId);
    rp1Uart0Puts("\n");


    /*
     * ------------------------------------------------------------------------
     * Locate MSI-X capability.
     * ------------------------------------------------------------------------
     */

    uint16_t msixCapability =
        findMsixCapability();

    if (msixCapability == 0u)
    {
        return -5;
    }


    /*
     * ------------------------------------------------------------------------
     * Configure interrupt path from GIC backwards toward RP1.
     * ------------------------------------------------------------------------
     *
     * Enable consumers before producers to avoid an interrupt arriving
     * before its parent controllers are ready.
     */

    /*
     * The endpoint emits MSI-X Memory Write TLPs to ff_ffff_f000.
     * Program the BCM2712 root-complex inbound BAR that maps that PCIe
     * address onto the MIP0 registers before enabling MSI-X at RP1.
     */
    pcie2ConfigureMip0InboundWindow();

    gicConfigureRp1Vector();

    mip0ConfigureVectorZero();


    if (configureRp1MsixVectorZero(
            msixCapability) != 0)
    {
        return -6;
    }


    /*
     * Enable RP1 interrupt source zero.
     *
     * The individual GPIO interrupt remains disabled until
     * rp1GpioEnablePcieInterrupt() is called later.
     */
    enableRp1IoBank0MsixSource();


    return 0;
}


/*
 * ============================================================================
 * RP1 identification
 * ============================================================================
 */

uint32_t rp1GetChipId(void)
{
    return rp1MmioRead32(
        RP1_SYSINFO_BASE);
}


/*
 * ============================================================================
 * GPIO helpers
 * ============================================================================
 */

static int validBank0Pin(unsigned pin)
{
    /*
     * Header GPIO pins are GPIO0-GPIO27.
     */
    return pin < 28u;
}


static uint64_t gpioCtrlAddress(unsigned pin)
{
    return RP1_IO_BANK0_BASE +
           RP1_GPIO_CTRL(pin);
}


/*
 * Configure a pin to use SYS_RIO rather than an alternate peripheral.
 */

static void configurePinForSysRio(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return;
    }

    uint64_t ctrl =
        gpioCtrlAddress(pin);


    /*
     * Clear:
     *
     *     function select
     *     output override
     *     output-enable override
     *     input override
     *     IRQ override
     */

    rp1MmioWrite32(
        ctrl +
        RP1_CLR_OFFSET,
        RP1_GPIO_FUNCSEL_MASK |
        RP1_GPIO_OUTOVER_MASK |
        RP1_GPIO_OEOVER_MASK |
        RP1_GPIO_INOVER_MASK |
        RP1_GPIO_IRQOVER_MASK);


    /*
     * Select SYS_RIO / GPIO function.
     */
    rp1MmioWrite32(
        ctrl +
        RP1_SET_OFFSET,
        RP1_GPIO_FUNC_GPIO);
}


/*
 * ============================================================================
 * GPIO output
 * ============================================================================
 */

void rp1GpioInitOutput(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return;
    }

    uint32_t bit =
        1u << pin;


    configurePinForSysRio(pin);


    /*
     * Start low before enabling the output driver.
     */
    rp1MmioWrite32(
        RP1_SYS_RIO0_BASE +
        RP1_CLR_OFFSET +
        RP1_RIO_OUT,
        bit);


    uint64_t pad =
        RP1_PADS_BANK0_BASE +
        RP1_PAD_GPIO(pin);


    /*
     * Disable pull-up/pull-down and permit output.
     */
    rp1MmioWrite32(
        pad +
        RP1_CLR_OFFSET,
        RP1_PAD_PULL_MASK |
        RP1_PAD_OUTPUT_DISABLE);


    /*
     * Keep input sensing enabled as well.
     */
    rp1MmioWrite32(
        pad +
        RP1_SET_OFFSET,
        RP1_PAD_INPUT_ENABLE);


    /*
     * Enable SYS_RIO output driver.
     */
    rp1MmioWrite32(
        RP1_SYS_RIO0_BASE +
        RP1_SET_OFFSET +
        RP1_RIO_OE,
        bit);
}


/*
 * ============================================================================
 * GPIO input
 * ============================================================================
 */

void rp1GpioInitInput(unsigned pin,
                      uint32_t pull)
{
    if (!validBank0Pin(pin))
    {
        return;
    }

    uint32_t bit =
        1u << pin;


    configurePinForSysRio(pin);


    /*
     * Disable SYS_RIO output.
     */
    rp1MmioWrite32(
        RP1_SYS_RIO0_BASE +
        RP1_CLR_OFFSET +
        RP1_RIO_OE,
        bit);


    uint64_t pad =
        RP1_PADS_BANK0_BASE +
        RP1_PAD_GPIO(pin);


    /*
     * Remove previous pull selection.
     */
    rp1MmioWrite32(
        pad +
        RP1_CLR_OFFSET,
        RP1_PAD_PULL_MASK);


    pull &= RP1_PAD_PULL_MASK;

    /* Input enabled, output pad disabled, and requested pull selected. */
    rp1MmioWrite32(
        pad +
        RP1_SET_OFFSET,
        RP1_PAD_INPUT_ENABLE |
        RP1_PAD_OUTPUT_DISABLE |
        pull);
}


void rp1GpioInitInputPullDown(unsigned pin)
{
    rp1GpioInitInput(pin, RP1_PAD_PULL_DOWN);
}


/*
 * ============================================================================
 * GPIO read/write
 * ============================================================================
 */

void rp1GpioWrite(unsigned pin,
                  int high)
{
    if (!validBank0Pin(pin))
    {
        return;
    }

    uint64_t alias =
        high
            ? RP1_SET_OFFSET
            : RP1_CLR_OFFSET;

    rp1MmioWrite32(
        RP1_SYS_RIO0_BASE +
        alias +
        RP1_RIO_OUT,
        1u << pin);
}


int rp1GpioRead(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return 0;
    }

    uint32_t input =
        rp1MmioRead32(
            RP1_SYS_RIO0_BASE +
            RP1_RIO_IN);

    return (int)(
        (input >> pin) &
        1u);
}


/*
 * ============================================================================
 * GPIO interrupt support
 * ============================================================================
 */

void rp1GpioDisablePcieInterrupt(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return;
    }

    rp1MmioWrite32(
        RP1_IO_BANK0_BASE +
        RP1_CLR_OFFSET +
        RP1_GPIO_PCIE_INTE,
        1u << pin);
}


void rp1GpioConfigureRisingInterrupt(unsigned pin)
{
    rp1GpioConfigureInterrupt(pin, RP1_GPIO_IRQ_RISING);
}


void rp1GpioConfigureInterrupt(unsigned pin,
                               uint32_t events)
{
    if (!validBank0Pin(pin))
    {
        return;
    }


    /*
     * Keep the downstream interrupt disabled while configuring it.
     */
    rp1GpioDisablePcieInterrupt(
        pin);


    uint64_t ctrl =
        gpioCtrlAddress(pin);


    /*
     * Remove all old event enables for this GPIO.
     */
    rp1MmioWrite32(
        ctrl +
        RP1_CLR_OFFSET,
        RP1_GPIO_IRQ_MASK);


    /*
     * Clear any previously latched edge.
     */
    rp1MmioWrite32(
        ctrl +
        RP1_SET_OFFSET,
        RP1_GPIO_IRQRESET);


    /* Enable the requested raw or filtered event detectors. */
    rp1MmioWrite32(
        ctrl +
        RP1_SET_OFFSET,
        events & RP1_GPIO_IRQ_MASK);


    /*
     * Read-back flushes posted PCIe writes.
     */
    (void)rp1MmioRead32(
        RP1_IO_BANK0_BASE +
        RP1_GPIO_PCIE_INTS);
}


void rp1GpioEnablePcieInterrupt(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return;
    }


    /*
     * Make this GPIO contribute to the IO_BANK0 PCIe interrupt.
     */
    rp1MmioWrite32(
        RP1_IO_BANK0_BASE +
        RP1_SET_OFFSET +
        RP1_GPIO_PCIE_INTE,
        1u << pin);


    /*
     * Flush posted write.
     */
    (void)rp1MmioRead32(
        RP1_IO_BANK0_BASE +
        RP1_GPIO_PCIE_INTS);
}


int rp1GpioInterruptPending(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return 0;
    }


    uint32_t pending =
        rp1MmioRead32(
            RP1_IO_BANK0_BASE +
            RP1_GPIO_PCIE_INTS);


    return (pending &
            (1u << pin)) != 0u;
}


void rp1GpioClearInterrupt(unsigned pin)
{
    if (!validBank0Pin(pin))
    {
        return;
    }


    /*
     * Clear the GPIO's latched edge event.
     */
    rp1MmioWrite32(
        gpioCtrlAddress(pin) +
        RP1_SET_OFFSET,
        RP1_GPIO_IRQRESET);


    /*
     * IMPORTANT:
     *
     * RP1 sits behind PCIe, so writes may be posted.
     *
     * Read the interrupt-status register to guarantee the GPIO clear has
     * reached RP1 before acknowledging the parent interrupt.
     */
    (void)rp1MmioRead32(
        RP1_IO_BANK0_BASE +
        RP1_GPIO_PCIE_INTS);
}


/*
 * ============================================================================
 * RP1 parent interrupt acknowledgement
 * ============================================================================
 */

void rp1AcknowledgeIoBank0(void)
{
    /*
     * IO_BANK0 is level-sensitive at the RP1 parent interrupt-controller
     * level.
     *
     * Because IACK_EN was enabled during initialization, write IACK after
     * servicing and clearing the GPIO child interrupt.
     */

    rp1MmioWrite32(
        RP1_PCIE_APBS_BASE +
        RP1_PCIE_REG_SET +
        RP1_MSIX_CFG(
            RP1_INT_IO_BANK0),
        RP1_MSIX_CFG_IACK);
}
