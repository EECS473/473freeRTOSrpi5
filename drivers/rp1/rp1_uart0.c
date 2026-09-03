#include "rp1_uart0.h"

#include "rp1.h"

#include <stdint.h>

/*
 * Raspberry Pi 5 RP1 UART0 console.
 *
 * RP1 BAR1 is left mapped by the boot firmware at 0x1c00000000 and UART0 is
 * at BAR1 offset 0x30000, giving the CPU-visible address 0x1c00030000.
 *
 * config.txt enables and initializes RP1 UART0 before the bare-metal kernel
 * starts, so this driver only performs polled PL011 transmit operations and
 * does not reconfigure the UART clock or baud rate.
 */


/*
 * ---------------------------------------------------------------------------
 * RP1 UART0 address
 * ---------------------------------------------------------------------------
 */

/*
 * CPU-visible RP1 UART0 address as configured by the Pi 5 firmware.
 *
 * Confirmed by the bootloader UART log:
 *
 *     RP1_UART 0000001c00030000
 */
#define RP1_UART0_BASE UINT64_C(0x1c00030000)
/*
 * ---------------------------------------------------------------------------
 * ARM PL011 register offsets
 * ---------------------------------------------------------------------------
 *
 * RP1 UART0 implements the ARM PL011 UART interface.
 */

#define PL011_DR_OFFSET          0x000u
#define PL011_RSR_ECR_OFFSET     0x004u
#define PL011_FR_OFFSET          0x018u
#define PL011_IBRD_OFFSET        0x024u
#define PL011_FBRD_OFFSET        0x028u
#define PL011_LCR_H_OFFSET       0x02cu
#define PL011_CR_OFFSET          0x030u
#define PL011_IFLS_OFFSET        0x034u
#define PL011_IMSC_OFFSET        0x038u
#define PL011_RIS_OFFSET         0x03cu
#define PL011_MIS_OFFSET         0x040u
#define PL011_ICR_OFFSET         0x044u


/*
 * ---------------------------------------------------------------------------
 * PL011 flag register bits
 * ---------------------------------------------------------------------------
 */

#define PL011_FR_CTS             (1u << 0)
#define PL011_FR_DSR             (1u << 1)
#define PL011_FR_DCD             (1u << 2)

#define PL011_FR_BUSY            (1u << 3)

#define PL011_FR_RXFE            (1u << 4)
#define PL011_FR_TXFF            (1u << 5)
#define PL011_FR_RXFF            (1u << 6)
#define PL011_FR_TXFE            (1u << 7)
#define PL011_FR_RI              (1u << 8)


/*
 * ============================================================================
 * MMIO helpers
 * ============================================================================
 */

static inline uint32_t rp1Uart0Read32(uint64_t address)
{
    uint32_t value;

    value =
        *(volatile uint32_t *)(uintptr_t)address;

    /*
     * Ensure device reads complete in the expected order.
     */
    __asm__ volatile(
        "dmb oshld"
        :
        :
        : "memory");

    return value;
}


static inline void rp1Uart0Write32(uint64_t address,
                               uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address =
        value;

    /*
     * Ensure the device write is issued before continuing.
     */
    __asm__ volatile(
        "dsb oshst"
        :
        :
        : "memory");
}


/*
 * ============================================================================
 * UART API
 * ============================================================================
 */

void rp1Uart0Init(void)
{
    /*
     * Intentionally empty.
     *
     * On Pi 5 the boot configuration contains:
     *
     *      enable_uart=1
     *      enable_rp1_uart=1
     *      pciex4_reset=0
     *
     * The boot firmware initializes RP1 UART0 and GPIO14/15 before handing
     * control to our bare-metal FreeRTOS image.
     *
     * Reprogramming IBRD/FBRD here is undesirable because doing so correctly
     * requires knowing the UART reference clock established by firmware.
     */
}


void rp1Uart0Putc(char c)
{
    /*
     * Wait until the transmit FIFO contains at least one free location.
     */
    while ((rp1Uart0Read32(
                RP1_UART0_BASE +
                PL011_FR_OFFSET) &
            PL011_FR_TXFF) != 0u)
    {
        /*
         * Busy wait.
         *
         * This console is intended for debug output, particularly before
         * the FreeRTOS scheduler is available.
         */
    }

    /*
     * PL011 DR uses the low eight bits for transmit data.
     */
    rp1Uart0Write32(
        RP1_UART0_BASE +
        PL011_DR_OFFSET,
        (uint32_t)(uint8_t)c);
}


void rp1Uart0Puts(const char *s)
{
    if (s == 0)
    {
        return;
    }

    while (*s != '\0')
    {
        /*
         * Most serial terminals expect CR/LF.
         */
        if (*s == '\n')
        {
            rp1Uart0Putc('\r');
        }

        rp1Uart0Putc(*s);

        ++s;
    }
}


/*
 * ============================================================================
 * Decimal output
 * ============================================================================
 */

void rp1Uart0PrintDec(uint64_t val)
{
    /*
     * Maximum uint64_t decimal value:
     *
     *      18446744073709551615
     *
     * which requires 20 characters.
     */
    char buffer[20];

    unsigned index = 0u;

    if (val == 0u)
    {
        rp1Uart0Putc('0');
        return;
    }

    while ((val != 0u) &&
           (index < sizeof(buffer)))
    {
        uint64_t digit =
            val % UINT64_C(10);

        buffer[index] =
            (char)('0' + digit);

        ++index;

        val /=
            UINT64_C(10);
    }

    /*
     * Digits were generated in reverse order.
     */
    while (index != 0u)
    {
        --index;

        rp1Uart0Putc(
            buffer[index]);
    }
}


/*
 * ============================================================================
 * Hexadecimal output
 * ============================================================================
 */

static const char hexDigits[] =
    "0123456789ABCDEF";


void rp1Uart0PrintHex8(uint8_t val)
{
    rp1Uart0Putc(
        hexDigits[
            (val >> 4) &
            0x0fu]);

    rp1Uart0Putc(
        hexDigits[
            val &
            0x0fu]);
}


void rp1Uart0PrintHex16(uint16_t val)
{
    for (int shift = 12;
         shift >= 0;
         shift -= 4)
    {
        rp1Uart0Putc(
            hexDigits[
                (val >> shift) &
                0x0fu]);
    }
}


void rp1Uart0PrintHex32(uint32_t val)
{
    for (int shift = 28;
         shift >= 0;
         shift -= 4)
    {
        rp1Uart0Putc(
            hexDigits[
                (val >> shift) &
                0x0fu]);
    }
}


void rp1Uart0PrintHex64(uint64_t val)
{
    for (int shift = 60;
         shift >= 0;
         shift -= 4)
    {
        rp1Uart0Putc(
            hexDigits[
                (val >> shift) &
                UINT64_C(0x0f)]);
    }
}
