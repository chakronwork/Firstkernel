#include <stdint.h>

#include "page_fault.h"
#include "console.h"
#include "serial.h"


/*
 * ============================================================
 * Read CR2
 * ============================================================
 *
 * CR2 contains the linear/virtual address
 * that caused the page fault.
 */
uint32_t page_fault_get_cr2(void)
{
    uint32_t value;

    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(value)
    );

    return value;
}


/*
 * ============================================================
 * VGA hexadecimal output
 * ============================================================
 */
static void console_write_hex32(
    uint32_t value
)
{
    static const char hex[] =
        "0123456789ABCDEF";

    char buffer[9];

    for (int i = 7; i >= 0; --i) {

        buffer[i] =
            hex[value & 0xF];

        value >>= 4;
    }

    buffer[8] = '\0';

    console_write(
        buffer
    );
}


/*
 * ============================================================
 * Print Page Fault status
 * ============================================================
 */
static void print_console_status(
    uint32_t error_code
)
{
    /*
     * Present bit.
     */
    console_write(
        "PRESENT:   "
    );

    if (error_code & 0x1U)
        console_write("YES\n");
    else
        console_write("NO\n");


    /*
     * Access type.
     */
    console_write(
        "ACCESS:    "
    );

    if (error_code & 0x2U)
        console_write("WRITE\n");
    else
        console_write("READ\n");


    /*
     * Privilege level.
     */
    console_write(
        "MODE:      "
    );

    if (error_code & 0x4U)
        console_write("USER\n");
    else
        console_write("SUPERVISOR\n");


    /*
     * Reserved bit violation.
     */
    console_write(
        "RSVD:      "
    );

    if (error_code & 0x8U)
        console_write("YES\n");
    else
        console_write("NO\n");


    /*
     * Instruction fetch.
     *
     * Bit 4 is available on processors that
     * support instruction-fetch page-fault
     * reporting.
     */
    console_write(
        "I/D:       "
    );

    if (error_code & 0x10U)
        console_write("INSTRUCTION\n");
    else
        console_write("DATA\n");
}


/*
 * ============================================================
 * Serial Page Fault status
 * ============================================================
 */
static void serial_write_status(
    uint32_t error_code
)
{
    serial_write(
        "PRESENT:   "
    );

    if (error_code & 0x1U)
        serial_write("YES\n");
    else
        serial_write("NO\n");


    serial_write(
        "ACCESS:    "
    );

    if (error_code & 0x2U)
        serial_write("WRITE\n");
    else
        serial_write("READ\n");


    serial_write(
        "MODE:      "
    );

    if (error_code & 0x4U)
        serial_write("USER\n");
    else
        serial_write("SUPERVISOR\n");


    serial_write(
        "RSVD:      "
    );

    if (error_code & 0x8U)
        serial_write("YES\n");
    else
        serial_write("NO\n");


    serial_write(
        "I/D:       "
    );

    if (error_code & 0x10U)
        serial_write("INSTRUCTION\n");
    else
        serial_write("DATA\n");
}


/*
 * ============================================================
 * Page Fault handler
 * ============================================================
 */
void page_fault_handler(
    struct registers *regs
)
{
    uint32_t cr2;


    if (regs == 0)
        return;


    cr2 =
        page_fault_get_cr2();


    /*
     * ==================================================
     * VGA
     * ==================================================
     */
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "\n\n"
        "========== PAGE FAULT ==========\n"
    );


    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "CR2:       0x"
    );

    console_write_hex32(
        cr2
    );

    console_write(
        "\nEIP:       0x"
    );

    console_write_hex32(
        regs->eip
    );

    console_write(
        "\nCS:        0x"
    );

    console_write_hex32(
        regs->cs
    );

    console_write(
        "\nERROR:     0x"
    );

    console_write_hex32(
        regs->err_code
    );

    console_write(
        "\n"
    );


    print_console_status(
        regs->err_code
    );


    console_write(
        "================================\n"
    );


    /*
     * ==================================================
     * Serial
     * ==================================================
     */
    serial_write(
        "\n\n"
        "========== PAGE FAULT ==========\n"
    );


    serial_write(
        "CR2:       "
    );

    serial_write_hex32(
        cr2
    );

    serial_write(
        "\nEIP:       "
    );

    serial_write_hex32(
        regs->eip
    );

    serial_write(
        "\nCS:        "
    );

    serial_write_hex32(
        regs->cs
    );

    serial_write(
        "\nERROR:     "
    );

    serial_write_hex32(
        regs->err_code
    );

    serial_write(
        "\n"
    );


    serial_write_status(
        regs->err_code
    );


    serial_write(
        "================================\n"
    );


    /*
     * Fatal for now.
     *
     * Later this function will become the
     * entry point for demand paging / VM
     * fault handling.
     */
    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );

    console_write(
        "Page fault is fatal. CPU halted.\n"
    );


    serial_write(
        "Page fault is fatal. CPU halted.\n"
    );


    for (;;) {

        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}