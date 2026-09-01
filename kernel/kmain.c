#include <stdint.h>

#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "kmalloc.h"
#include "paging.h"
#include "serial.h"


#define MULTIBOOT_MAGIC 0x2BADB002

#define COMMAND_BUFFER_SIZE 128

#define HEAP_STRESS_SLOTS 64
#define HEAP_STRESS_ROUNDS 10


/*
 * ============================================================
 * CPU halt
 * ============================================================
 */
static void halt_cpu(void)
{
    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}


/*
 * ============================================================
 * Decimal output
 * ============================================================
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


/*
 * ============================================================
 * Heap stress-test state
 * ============================================================
 */
static void *stress_ptrs[
    HEAP_STRESS_SLOTS
];


static uint32_t stress_sizes[
    HEAP_STRESS_SLOTS
];


/*
 * ============================================================
 * Deterministic pseudo-random generator
 * ============================================================
 */
static uint32_t stress_next(
    uint32_t value
)
{
    return
        value * 1664525U +
        1013904223U;
}


/*
 * ============================================================
 * Kernel heap stress test
 * ============================================================
 */
static int heap_stress_test(void)
{
    uint32_t seed =
        0x12345678U;


    /*
     * Repeat the complete test.
     */
    for (
        uint32_t round = 0;
        round < HEAP_STRESS_ROUNDS;
        ++round
    ) {

        /*
         * ==================================================
         * Phase 1
         *
         * Allocate many blocks.
         * ==================================================
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            ++i
        ) {

            seed =
                stress_next(seed);


            /*
             * 8 .. 512 bytes.
             */
            uint32_t size =
                8U +
                (seed % 505U);


            stress_sizes[i] =
                size;


            stress_ptrs[i] =
                kmalloc(size);


            if (stress_ptrs[i] == 0)
                return 0;


            /*
             * Fill memory with deterministic pattern.
             */
            uint8_t pattern =
                (uint8_t)(
                    0xA0U +
                    (i & 0x0FU)
                );


            uint8_t *bytes =
                (uint8_t *)stress_ptrs[i];


            for (
                uint32_t j = 0;
                j < size;
                ++j
            ) {
                bytes[j] =
                    pattern;
            }
        }


        /*
         * ==================================================
         * Phase 2
         *
         * Verify all blocks.
         * ==================================================
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            ++i
        ) {

            uint8_t pattern =
                (uint8_t)(
                    0xA0U +
                    (i & 0x0FU)
                );


            uint8_t *bytes =
                (uint8_t *)stress_ptrs[i];


            for (
                uint32_t j = 0;
                j < stress_sizes[i];
                ++j
            ) {

                if (bytes[j] != pattern)
                    return 0;
            }
        }


        /*
         * Verify heap metadata.
         */
        if (!kmalloc_validate())
            return 0;


        /*
         * ==================================================
         * Phase 3
         *
         * Free every second block.
         *
         * Creates fragmentation.
         * ==================================================
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            i += 2
        ) {

            kfree(
                stress_ptrs[i]
            );

            stress_ptrs[i] = 0;
        }


        /*
         * Heap must remain valid.
         */
        if (!kmalloc_validate())
            return 0;


        /*
         * ==================================================
         * Phase 4
         *
         * Reuse freed blocks with different sizes.
         * ==================================================
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            i += 2
        ) {

            uint32_t size =
                16U +
                ((i * 13U) % 200U);


            stress_sizes[i] =
                size;


            stress_ptrs[i] =
                kmalloc(size);


            if (stress_ptrs[i] == 0)
                return 0;


            uint8_t pattern =
                (uint8_t)(
                    0x50U +
                    (i & 0x0FU)
                );


            uint8_t *bytes =
                (uint8_t *)stress_ptrs[i];


            for (
                uint32_t j = 0;
                j < size;
                ++j
            ) {
                bytes[j] =
                    pattern;
            }


            /*
             * Verify immediately.
             */
            for (
                uint32_t j = 0;
                j < size;
                ++j
            ) {

                if (bytes[j] != pattern)
                    return 0;
            }
        }


        /*
         * Verify heap again.
         */
        if (!kmalloc_validate())
            return 0;


        /*
         * ==================================================
         * Phase 5
         *
         * Free everything.
         * ==================================================
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            ++i
        ) {

            kfree(
                stress_ptrs[i]
            );

            stress_ptrs[i] = 0;
        }


        /*
         * Final validation for this round.
         */
        if (!kmalloc_validate())
            return 0;
    }


    return 1;
}


/*
 * ============================================================
 * Kernel entry point
 * ============================================================
 */
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
        "firstOS v0.0.13\n"
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
     * PMM is initialized before STI.
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
     * Kernel Heap initialization
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
     * Basic kmalloc allocation test
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
     * Write test data.
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
     * kfree + reuse test
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
     * Heap stress test
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] heap stress test\n"
    );

    serial_write(
        "[test] heap stress test\n"
    );


    if (!heap_stress_test()) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] heap integrity/stress test\n"
        );

        serial_write(
            "[fail] heap integrity/stress test\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] heap stress test passed\n"
    );

    serial_write(
        "[ ok ] heap stress test passed\n"
    );


    /*
     * Final heap validation.
     */
    if (!kmalloc_validate()) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] final heap validation\n"
        );

        serial_write(
            "[fail] final heap validation\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] final heap validation passed\n"
    );

    serial_write(
        "[ ok ] final heap validation passed\n"
    );


    /*
     * ==================================================
     * Heap statistics
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
     * Paging initialization
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] initializing paging\n"
    );

    serial_write(
        "[test] initializing paging\n"
    );


    /*
     * Build identity-mapped page tables.
     */
    if (!paging_init()) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] paging initialization\n"
        );

        serial_write(
            "[fail] paging initialization\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] page directory initialized\n"
    );

    serial_write(
        "[ ok ] page directory initialized\n"
    );


    console_write(
        "[ ok ] page tables initialized\n"
    );

    serial_write(
        "[ ok ] page tables initialized\n"
    );


    /*
     * Enable paging.
     *
     * Interrupts are still disabled.
     */
    paging_enable();


    /*
     * Verify CR0.PG.
     */
    if (!paging_is_enabled()) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] paging enable\n"
        );

        serial_write(
            "[fail] paging enable\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] paging enabled\n"
    );

    serial_write(
        "[ ok ] paging enabled\n"
    );


    /*
     * Display mapped page count.
     */
    console_write(
        "[info] mapped pages: "
    );

    print_uint32(
        paging_get_mapped_pages()
    );

    console_write(
        "\n"
    );


    serial_write(
        "[info] mapped pages: "
    );

    serial_write_dec(
        paging_get_mapped_pages()
    );

    serial_write(
        "\n"
    );


    /*
     * ==================================================
     * Paging memory access test
     * ==================================================
     *
     * This variable lives in kernel BSS/data.
     *
     * Since the current paging implementation
     * is identity-mapped, the variable's linear
     * address remains valid.
     */
    volatile uint32_t paging_test =
        0x12345678U;


    if (paging_test != 0x12345678U) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] paging memory access\n"
        );

        serial_write(
            "[fail] paging memory access\n"
        );

        halt_cpu();
    }


    paging_test =
        0xDEADBEEFU;


    if (paging_test != 0xDEADBEEFU) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] paging write/read test\n"
        );

        serial_write(
            "[fail] paging write/read test\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] paging memory access test\n"
    );

    serial_write(
        "[ ok ] paging memory access test\n"
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
 * Paging subsystem test
 * ==================================================
 */

console_set_color(
    VGA_YELLOW,
    VGA_BLACK
);

console_write(
    "[test] paging map/unmap subsystem\n"
);

serial_write(
    "[test] paging map/unmap subsystem\n"
);


/*
 * Use a virtual page inside the existing
 * identity-mapped 128 MiB range.
 *
 * 8 MiB is safely outside the small kernel
 * image while remaining inside the bootstrap map.
 */
#define PAGING_TEST_VIRTUAL 0x00800000U


/*
 * Find the existing identity mapping.
 *
 * We will temporarily replace it.
 */
uint32_t original_physical = 0;
uint32_t original_flags = 0;

if (!paging_get_page(
        PAGING_TEST_VIRTUAL,
        &original_physical,
        &original_flags
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] get_page before remap\n"
    );

    serial_write(
        "[fail] get_page before remap\n"
    );

    halt_cpu();
}


/*
 * Allocate one physical test page.
 */
uint32_t test_physical =
    pmm_alloc_page();


if (test_physical == 0) {

    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] test physical page allocation\n"
    );

    serial_write(
        "[fail] test physical page allocation\n"
    );

    halt_cpu();
}


/*
 * Map virtual page to the new physical page.
 */
if (!paging_map_page(
        PAGING_TEST_VIRTUAL,
        test_physical,
        PAGE_WRITABLE
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] map_page\n"
    );

    serial_write(
        "[fail] map_page\n"
    );

    pmm_free_page(
        test_physical
    );

    halt_cpu();
}


console_set_color(
    VGA_LGREEN,
    VGA_BLACK
);

console_write(
    "[ ok ] map_page\n"
);

serial_write(
    "[ ok ] map_page\n"
);


/*
 * Verify page-table result.
 */
uint32_t mapped_physical = 0;
uint32_t mapped_flags = 0;

if (!paging_get_page(
        PAGING_TEST_VIRTUAL,
        &mapped_physical,
        &mapped_flags
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] get_page after map\n"
    );

    serial_write(
        "[fail] get_page after map\n"
    );

    halt_cpu();
}


if (
    mapped_physical != test_physical
)
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] physical mapping mismatch\n"
    );

    serial_write(
        "[fail] physical mapping mismatch\n"
    );

    halt_cpu();
}


console_write(
    "[ ok ] get_page\n"
);

serial_write(
    "[ ok ] get_page\n"
);


/*
 * Access the virtual address.
 *
 * The CPU should now translate:
 *
 *     PAGING_TEST_VIRTUAL
 *
 * to:
 *
 *     test_physical
 */
volatile uint32_t *test_virtual =
    (volatile uint32_t *)(
        uintptr_t
    )PAGING_TEST_VIRTUAL;


*test_virtual =
    0xCAFEBABEU;


if (*test_virtual != 0xCAFEBABEU) {

    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] mapped virtual memory access\n"
    );

    serial_write(
        "[fail] mapped virtual memory access\n"
    );

    halt_cpu();
}


console_write(
    "[ ok ] mapped virtual memory access\n"
);

serial_write(
    "[ ok ] mapped virtual memory access\n"
);


/*
 * Unmap.
 */
if (!paging_unmap_page(
        PAGING_TEST_VIRTUAL
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] unmap_page\n"
    );

    serial_write(
        "[fail] unmap_page\n"
    );

    halt_cpu();
}


console_write(
    "[ ok ] unmap_page\n"
);

serial_write(
    "[ ok ] unmap_page\n"
);


/*
 * Verify that page is now unmapped.
 */
if (paging_get_page(
        PAGING_TEST_VIRTUAL,
        &mapped_physical,
        &mapped_flags
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] page still mapped\n"
    );

    serial_write(
        "[fail] page still mapped\n"
    );

    halt_cpu();
}


console_write(
    "[ ok ] page unmapped\n"
);

serial_write(
    "[ ok ] page unmapped\n"
);


/*
 * Restore original identity mapping.
 */
if (!paging_map_page(
        PAGING_TEST_VIRTUAL,
        original_physical,
        original_flags
    ))
{
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] restore original mapping\n"
    );

    serial_write(
        "[fail] restore original mapping\n"
    );

    halt_cpu();
}


console_write(
    "[ ok ] original mapping restored\n"
);

serial_write(
    "[ ok ] original mapping restored\n"
);


/*
 * Release the temporary physical page.
 */
pmm_free_page(
    test_physical
);


console_write(
    "[ ok ] test physical page released\n"
);

serial_write(
    "[ ok ] test physical page released\n"
);


console_set_color(
    VGA_LGREEN,
    VGA_BLACK
);

console_write(
    "[ ok ] paging subsystem test passed\n"
);

serial_write(
    "[ ok ] paging subsystem test passed\n"
);

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