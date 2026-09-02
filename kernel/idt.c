#include <stdint.h>

#include "idt.h"
#include "page_fault.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "console.h"
#include "serial.h"
#include "task.h"


/*
 * ============================================================
 * IDT entry
 * ============================================================
 */
struct idt_entry
{
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));


/*
 * ============================================================
 * IDTR
 * ============================================================
 */
struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


/*
 * ============================================================
 * IDT storage
 * ============================================================
 */
static struct idt_entry idt[256];

static struct idt_ptr idtp;


/*
 * ============================================================
 * ISR / IDT assembly functions
 * ============================================================
 */

extern void idt_flush(
    uint32_t
);


/*
 * CPU exceptions
 */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);


/*
 * Hardware IRQs
 */
extern void isr32(void);
extern void isr33(void);


/*
 * Software scheduler interrupt
 *
 * int 0x30
 */
extern void isr48(void);


/*
 * ============================================================
 * Console hexadecimal output
 * ============================================================
 */
static void console_write_hex32(
    uint32_t value
)
{
    static const char hex[] =
        "0123456789ABCDEF";

    char buffer[9];


    for (
        int i = 7;
        i >= 0;
        --i
    ) {

        buffer[i] =
            hex[value & 0x0FU];

        value >>=
            4;
    }


    buffer[8] =
        '\0';


    console_write(
        buffer
    );
}


/*
 * ============================================================
 * Serial hexadecimal output
 * ============================================================
 */
static void serial_write_hex32_line(
    const char *name,
    uint32_t value
)
{
    serial_write(
        name
    );

    serial_write(
        "0x"
    );


    static const char hex[] =
        "0123456789ABCDEF";

    char buffer[9];


    for (
        int i = 7;
        i >= 0;
        --i
    ) {

        buffer[i] =
            hex[value & 0x0FU];

        value >>=
            4;
    }


    buffer[8] =
        '\0';


    serial_write(
        buffer
    );

    serial_write(
        "\n"
    );
}


/*
 * ============================================================
 * Set IDT gate
 * ============================================================
 */
static void idt_set_gate(
    uint8_t num,
    uint32_t base,
    uint16_t selector,
    uint8_t flags
)
{
    idt[num].base_low =
        (uint16_t)(
            base &
            0xFFFFU
        );


    idt[num].base_high =
        (uint16_t)(
            (base >> 16) &
            0xFFFFU
        );


    idt[num].selector =
        selector;


    idt[num].zero =
        0;


    idt[num].flags =
        flags;
}


/*
 * ============================================================
 * IRQ dispatcher
 * ============================================================
 *
 * Returns the interrupt frame that the assembly ISR must
 * restore.
 *
 * Normally this is the current frame.
 *
 * For IRQ0, the scheduler may return another task's frame.
 */
static struct registers *irq_handler(
    struct registers *regs
)
{
    if (regs == 0) {
        return regs;
    }


    uint32_t irq =
        regs->int_no - 32U;


    switch (irq) {

        /*
         * ==================================================
         * IRQ0
         *
         * PIT timer
         * ==================================================
         */
        case 0:

            /*
             * Update PIT tick counter and acknowledge
             * IRQ0.
             */
            timer_handler();


            /*
             * Run preemptive scheduler.
             *
             * This may return the current task's frame
             * or another task's saved frame.
             */
            return
                task_scheduler_tick(
                    regs
                );


        /*
         * ==================================================
         * IRQ1
         *
         * PS/2 keyboard
         * ==================================================
         */
        case 1:

            keyboard_handler();

            return regs;


        /*
         * ==================================================
         * Other IRQs
         * ==================================================
         */
        default:

            pic_send_eoi(
                (uint8_t)irq
            );

            return regs;
    }
}


/*
 * ============================================================
 * Main interrupt dispatcher
 * ============================================================
 *
 * IMPORTANT:
 *
 * The return value is the interrupt frame that idt.s must
 * restore.
 *
 * Normally:
 *
 *     return regs;
 *
 * During scheduling:
 *
 *     return another_task_frame;
 */
struct registers *isr_handler(
    struct registers *regs
)
{
    /*
     * Invalid frame.
     */
    if (regs == 0) {
        return regs;
    }


    /*
     * ========================================================
     * Hardware IRQs
     * ========================================================
     *
     * PIC is remapped:
     *
     *   IRQ0 -> vector 32
     *   IRQ1 -> vector 33
     *   ...
     *   IRQ15 -> vector 47
     */
    if (
        regs->int_no >= 32U &&
        regs->int_no <= 47U
    ) {

        return
            irq_handler(
                regs
            );
    }


    /*
     * ========================================================
     * Software scheduler interrupt
     * ========================================================
     *
     * int 0x30
     *
     * Used by task_yield().
     */
    if (
        regs->int_no == 48U
    ) {

        return
            task_scheduler_tick(
                regs
            );
    }


    /*
     * ========================================================
     * Page Fault
     * ========================================================
     *
     * Exception 14.
     */
    if (
        regs->int_no == 14U
    ) {

        page_fault_handler(
            regs
        );


        /*
         * Page fault handler normally does not return
         * for fatal faults.
         *
         * Return the same frame for completeness.
         */
        return regs;
    }


    /*
     * ========================================================
     * Generic CPU exception
     * ========================================================
     */

    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );


    console_write(
        "\n\n"
        "========== CPU EXCEPTION ==========\n"
    );


    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );


    /*
     * Interrupt number.
     */
    console_write(
        "INTERRUPT: 0x"
    );

    console_write_hex32(
        regs->int_no
    );


    /*
     * Error code.
     */
    console_write(
        "\nERROR:     0x"
    );

    console_write_hex32(
        regs->err_code
    );


    /*
     * EIP.
     */
    console_write(
        "\nEIP:       0x"
    );

    console_write_hex32(
        regs->eip
    );


    /*
     * CS.
     */
    console_write(
        "\nCS:        0x"
    );

    console_write_hex32(
        regs->cs
    );


    /*
     * EFLAGS.
     */
    console_write(
        "\nEFLAGS:    0x"
    );

    console_write_hex32(
        regs->eflags
    );


    console_write(
        "\n"
    );


    /*
     * ========================================================
     * Serial diagnostics
     * ========================================================
     */

    serial_write(
        "\n\n"
        "========== CPU EXCEPTION ==========\n"
    );


    serial_write_hex32_line(
        "INTERRUPT: ",
        regs->int_no
    );


    serial_write_hex32_line(
        "ERROR:     ",
        regs->err_code
    );


    serial_write_hex32_line(
        "EIP:       ",
        regs->eip
    );


    serial_write_hex32_line(
        "CS:        ",
        regs->cs
    );


    serial_write_hex32_line(
        "EFLAGS:    ",
        regs->eflags
    );


    serial_write(
        "===================================\n"
    );


    /*
     * Restore default console color.
     */
    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );


    console_write(
        "CPU halted.\n"
    );


    serial_write(
        "CPU halted.\n"
    );


    /*
     * Generic exceptions are fatal.
     */
    for (;;) {

        __asm__ volatile (
            "cli"
        );

        __asm__ volatile (
            "hlt"
        );
    }


    /*
     * Unreachable.
     */
    return regs;
}


/*
 * ============================================================
 * Initialize IDT
 * ============================================================
 */
void idt_init(void)
{
    /*
     * ========================================================
     * Configure IDTR
     * ========================================================
     */
    idtp.limit =
        sizeof(idt) - 1U;

    idtp.base =
        (uint32_t)&idt[0];


    /*
     * ========================================================
     * Clear all IDT entries
     * ========================================================
     */
    for (
        int i = 0;
        i < 256;
        ++i
    ) {

        idt[i].base_low =
            0;

        idt[i].base_high =
            0;

        idt[i].selector =
            0;

        idt[i].zero =
            0;

        idt[i].flags =
            0;
    }


    /*
     * ========================================================
     * CPU exception ISR table
     * ========================================================
     */
    uint32_t isrs[32] = {

        (uint32_t)isr0,
        (uint32_t)isr1,
        (uint32_t)isr2,
        (uint32_t)isr3,
        (uint32_t)isr4,
        (uint32_t)isr5,
        (uint32_t)isr6,
        (uint32_t)isr7,

        (uint32_t)isr8,
        (uint32_t)isr9,
        (uint32_t)isr10,
        (uint32_t)isr11,
        (uint32_t)isr12,
        (uint32_t)isr13,
        (uint32_t)isr14,
        (uint32_t)isr15,

        (uint32_t)isr16,
        (uint32_t)isr17,
        (uint32_t)isr18,
        (uint32_t)isr19,
        (uint32_t)isr20,
        (uint32_t)isr21,
        (uint32_t)isr22,
        (uint32_t)isr23,

        (uint32_t)isr24,
        (uint32_t)isr25,
        (uint32_t)isr26,
        (uint32_t)isr27,
        (uint32_t)isr28,
        (uint32_t)isr29,
        (uint32_t)isr30,
        (uint32_t)isr31
    };


    /*
     * ========================================================
     * Install CPU exception gates
     * ========================================================
     *
     * Kernel code segment:
     *
     *     0x08
     *
     * Interrupt gate:
     *
     *     0x8E
     */
    for (
        int i = 0;
        i < 32;
        ++i
    ) {

        idt_set_gate(
            (uint8_t)i,
            isrs[i],
            0x08,
            0x8E
        );
    }


    /*
     * ========================================================
     * IRQ0
     *
     * Vector 32
     * ========================================================
     */
    idt_set_gate(
        32,
        (uint32_t)isr32,
        0x08,
        0x8E
    );


    /*
     * ========================================================
     * IRQ1
     *
     * Vector 33
     * ========================================================
     */
    idt_set_gate(
        33,
        (uint32_t)isr33,
        0x08,
        0x8E
    );


    /*
     * ========================================================
     * Software scheduler interrupt
     *
     * int 0x30
     *
     * Vector 48
     * ========================================================
     */
    idt_set_gate(
        48,
        (uint32_t)isr48,
        0x08,
        0x8E
    );


    /*
     * ========================================================
     * Load IDT
     * ========================================================
     */
    idt_flush(
        (uint32_t)&idtp
    );
}