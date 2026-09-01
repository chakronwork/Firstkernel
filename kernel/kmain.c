#include <stdint.h>

#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "kmalloc.h"
#include "serial.h"


#define MULTIBOOT_MAGIC 0x2BADB002

#define COMMAND_BUFFER_SIZE 128


/*
 * Halt CPU permanently.
 */
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
static void print_uint32(
    uint32_t value
)
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

        console_putc(
            buffer[--i]
        );
    }
}


void kmain(
    uint32_t magic,
    uint32_t mbi_addr
)
{
    char command[
        COMMAND_BUFFER_SIZE
    ];


    /*
     * ==================================================
     * Serial debug console
     * ==================================================
     */
    serial_init();

    serial_write(
        "\n"
        "========================================\n"
        " firstOS serial console\n"
        "========================================\n"
    );


    /*
     * ==================================================
     * VGA console
     * ==================================================
     */
    console_init();

    console_set_color(
        VGA_LCYAN,
        VGA_BLACK
    );

    console_write(
        "firstOS v0.0.11\n"
    );

    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );

    console_write(
        "----------------------------------------\n"
    );


    /*
     * ==================================================
     * Multiboot verification
     * ==================================================
     */
    if (magic != MULTIBOOT_MAGIC) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "FATAL: bad multiboot magic\n"
        );

        serial_write(
            "[fatal] bad multiboot magic\n"
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


    serial_write(
        "[ ok ] multiboot verified\n"
    );

    serial_write(
        "[ ok ] kernel console ready\n"
    );


    /*
     * ==================================================
     * GDT
     * ==================================================
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


    serial_write(
        "[ ok ] gdt loaded\n"
    );

    serial_write(
        "[ ok ] kernel segments ready\n"
    );


    /*
     * ==================================================
     * IDT
     * ==================================================
     */
    idt_init();

    console_write(
        "[ ok ] idt loaded\n"
    );

    console_write(
        "[ ok ] exception handlers installed\n"
    );


    serial_write(
        "[ ok ] idt loaded\n"
    );

    serial_write(
        "[ ok ] exception handlers installed\n"
    );


    /*
     * ==================================================
     * PIC
     * ==================================================
     */
    pic_remap(
        32,
        40
    );

    console_write(
        "[ ok ] pic remapped: 32/40\n"
    );

    serial_write(
        "[ ok ] pic remapped: 32/40\n"
    );


    /*
     * ==================================================
     * PIT
     * ==================================================
     */
    timer_init(100);

    console_write(
        "[ ok ] pit timer configured: 100 hz\n"
    );

    serial_write(
        "[ ok ] pit timer configured: 100 hz\n"
    );


    /*
     * ==================================================
     * Keyboard
     * ==================================================
     */
    keyboard_init();

    console_write(
        "[ ok ] keyboard irq1 ready\n"
    );

    serial_write(
        "[ ok ] keyboard irq1 ready\n"
    );


    /*
     * ==================================================
     * Physical Memory Manager
     *
     * IMPORTANT:
     *
     * PMM is initialized BEFORE STI.
     * ==================================================
     */
    pmm_init(
        mbi_addr
    );

    console_write(
        "[ ok ] physical memory manager initialized\n"
    );

    serial_write(
        "[ ok ] physical memory manager initialized\n"
    );


    /*
     * ==================================================
     * PMM statistics
     * ==================================================
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


    serial_write(
        "[info] total pages: "
    );

    serial_write_dec(
        pmm_get_total_pages()
    );

    serial_write(
        "\n"
    );


    serial_write(
        "[info] free pages:  "
    );

    serial_write_dec(
        pmm_get_free_pages()
    );

    serial_write(
        "\n"
    );


    /*
     * ==================================================
     * PMM allocation test
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] allocating physical pages\n"
    );

    serial_write(
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

        serial_write(
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


    serial_write(
        "[ ok ] page A allocated\n"
    );

    serial_write(
        "[ ok ] page B allocated\n"
    );


    /*
     * ==================================================
     * Free PMM pages
     * ==================================================
     */
    pmm_free_page(
        page_a
    );

    pmm_free_page(
        page_b
    );


    console_write(
        "[ ok ] pages freed\n"
    );

    serial_write(
        "[ ok ] pages freed\n"
    );


    /*
     * ==================================================
     * Kernel Heap
     * ==================================================
     */
    kmalloc_init();

    console_write(
        "[ ok ] kernel heap initialized\n"
    );

    serial_write(
        "[ ok ] kernel heap initialized\n"
    );


    /*
     * ==================================================
     * Kernel Heap allocation test
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] kernel heap allocation\n"
    );

    serial_write(
        "[test] kernel heap allocation\n"
    );


    char *heap_a =
        (char *)kmalloc(32);

    char *heap_b =
        (char *)kmalloc(128);

    char *heap_c =
        (char *)kmalloc(512);


    /*
     * Allocation failure.
     */
    if (heap_a == 0 ||
        heap_b == 0 ||
        heap_c == 0)
    {
        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] kernel heap allocation\n"
        );

        serial_write(
            "[fail] kernel heap allocation\n"
        );

        halt_cpu();
    }


    /*
     * Write test data into allocated memory.
     */
    heap_a[0] = 'A';
    heap_a[1] = '\0';

    heap_b[0] = 'B';
    heap_b[1] = '\0';

    heap_c[0] = 'C';
    heap_c[1] = '\0';


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] kmalloc(32)\n"
    );

    console_write(
        "[ ok ] kmalloc(128)\n"
    );

    console_write(
        "[ ok ] kmalloc(512)\n"
    );


    serial_write(
        "[ ok ] kmalloc(32)\n"
    );

    serial_write(
        "[ ok ] kmalloc(128)\n"
    );

    serial_write(
        "[ ok ] kmalloc(512)\n"
    );


    /*
     * ==================================================
     * kfree test
     * ==================================================
     */
    kfree(
        heap_b
    );

    console_write(
        "[ ok ] kfree(128)\n"
    );

    serial_write(
        "[ ok ] kfree(128)\n"
    );


    /*
     * Allocate again.
     *
     * This should reuse the free block.
     */
    void *heap_d =
        kmalloc(64);

    if (heap_d == 0) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] heap block reuse\n"
        );

        serial_write(
            "[fail] heap block reuse\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] heap block reuse\n"
    );

    serial_write(
        "[ ok ] heap block reuse\n"
    );


    /*
     * ==================================================
     * Kernel Heap statistics
     * ==================================================
     */
    console_write(
        "[info] heap pages: "
    );

    print_uint32(
        kmalloc_get_pages()
    );

    console_write(
        "\n"
    );


    serial_write(
        "[info] heap pages: "
    );

    serial_write_dec(
        kmalloc_get_pages()
    );

    serial_write(
        "\n"
    );


    console_write(
        "[info] heap used:  "
    );

    print_uint32(
        kmalloc_get_used()
    );

    console_write(
        "\n"
    );


    serial_write(
        "[info] heap used:  "
    );

    serial_write_dec(
        kmalloc_get_used()
    );

    serial_write(
        "\n"
    );


    console_write(
        "[info] heap free:  "
    );

    print_uint32(
        kmalloc_get_free()
    );

    console_write(
        "\n"
    );


    serial_write(
        "[info] heap free:  "
    );

    serial_write_dec(
        kmalloc_get_free()
    );

    serial_write(
        "\n"
    );


    /*
     * ==================================================
     * Enable hardware interrupts
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[ ok ] enabling interrupts\n"
    );

    serial_write(
        "[ ok ] enabling interrupts\n"
    );


    __asm__ volatile ("sti");


    /*
     * ==================================================
     * Command line
     * ==================================================
     */
    console_set_color(
        VGA_LGREY,
        VGA_BLACK
    );

    console_write(
        "\nfirstos> "
    );

    serial_write(
        "\nfirstos> "
    );


    /*
     * ==================================================
     * Basic command input loop
     * ==================================================
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


            serial_write(
                "\nreceived: "
            );

            serial_write(
                command
            );
        }


        console_write(
            "\nfirstos> "
        );

        serial_write(
            "\nfirstos> "
        );
    }
}