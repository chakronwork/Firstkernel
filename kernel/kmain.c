#include <stdint.h>

#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"

#define MULTIBOOT_MAGIC 0x2BADB002

#define COMMAND_BUFFER_SIZE 128


static void halt_cpu(void)
{
    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}


/*
 * Print unsigned 32-bit integer.
 */
static void print_uint32(uint32_t value)
{
    char buffer[11];
    uint32_t i = 0;


    if (value == 0) {
        console_putc('0');
        return;
    }


    while (value > 0) {

        buffer[i++] =
            (char)('0' + (value % 10));

        value /= 10;
    }


    while (i > 0) {
        console_putc(buffer[--i]);
    }
}


void kmain(
    uint32_t magic,
    uint32_t mbi_addr
)
{
    char command[COMMAND_BUFFER_SIZE];


    /*
     * Console
     */
    console_init();

    console_set_color(
        VGA_LCYAN,
        VGA_BLACK
    );

    console_write(
        "firstOS v0.0.9\n"
    );

    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );

    console_write(
        "----------------------------------------\n"
    );


    /*
     * Multiboot verification.
     */
    if (magic != MULTIBOOT_MAGIC) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "FATAL: bad multiboot magic\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] multiboot verified\n"
    );

    console_write(
        "[ ok ] kernel console ready\n"
    );


    /*
     * GDT
     */
    gdt_init();

    console_write(
        "[ ok ] gdt loaded\n"
    );

    console_write(
        "[ ok ] kernel code segment ready\n"
    );

    console_write(
        "[ ok ] kernel data segment ready\n"
    );


    /*
     * IDT
     */
    idt_init();

    console_write(
        "[ ok ] idt loaded\n"
    );

    console_write(
        "[ ok ] exception handlers installed\n"
    );


    /*
     * PIC
     */
    pic_remap(
        32,
        40
    );

    console_write(
        "[ ok ] pic remapped: 32/40\n"
    );


    /*
     * PIT
     */
    timer_init(100);

    console_write(
        "[ ok ] pit timer configured: 100 hz\n"
    );


    /*
     * Keyboard
     */
    keyboard_init();

    console_write(
        "[ ok ] keyboard irq1 ready\n"
    );


    /*
     * Physical Memory Manager
     *
     * IMPORTANT:
     *
     * PMM is initialized BEFORE STI.
     */
    pmm_init(mbi_addr);

    console_write(
        "[ ok ] physical memory manager initialized\n"
    );


    /*
     * Display PMM statistics.
     */
    console_write(
        "[info] total pages: "
    );

    print_uint32(
        pmm_get_total_pages()
    );

    console_write(
        "\n"
    );


    console_write(
        "[info] free pages:  "
    );

    print_uint32(
        pmm_get_free_pages()
    );

    console_write(
        "\n"
    );


    /*
     * PMM allocation test.
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] allocating physical pages\n"
    );


    uint32_t page_a =
        pmm_alloc_page();

    uint32_t page_b =
        pmm_alloc_page();


    /*
     * Allocation failure.
     */
    if (page_a == 0 ||
        page_b == 0)
    {
        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] physical page allocation\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] page A allocated\n"
    );

    console_write(
        "[ ok ] page B allocated\n"
    );


    /*
     * Free pages.
     */
    pmm_free_page(page_a);
    pmm_free_page(page_b);

    console_write(
        "[ ok ] pages freed\n"
    );


    /*
     * Enable hardware interrupts.
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[ ok ] enabling interrupts\n"
    );

    __asm__ volatile ("sti");


    /*
     * Start command line.
     */
    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );

    console_write(
        "\nfirstos> "
    );


    /*
     * Basic command input loop.
     */
    for (;;) {

        int length =
            keyboard_readline(
                command,
                COMMAND_BUFFER_SIZE
            );


        if (length > 0) {

            console_write(
                "\nreceived: "
            );

            console_write(
                command
            );
        }


        console_write(
            "\nfirstos> "
        );
    }
}