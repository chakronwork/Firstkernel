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
#include "vmm.h"
#include "address_space.h"
#include "task.h"
#include "serial.h"


#define MULTIBOOT_MAGIC 0x2BADB002U

#define COMMAND_BUFFER_SIZE 128

#define HEAP_STRESS_SLOTS 64
#define HEAP_STRESS_ROUNDS 10

#define PAGE_FAULT_TEST_ADDRESS 0xF0000000U

#define PAGING_TEST_VIRTUAL 0x00800000U

#define ADDRESS_SPACE_TEST_VIRTUAL 0x40000000U


/*
 * ============================================================
 * Task test state
 * ============================================================
 */
static volatile uint32_t task_a_runs = 0;

static volatile uint32_t task_b_runs = 0;


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
 * Print unsigned decimal
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
            (char)(
                '0' +
                (value % 10U)
            );

        value /= 10U;
    }


    while (i > 0) {

        console_putc(
            buffer[--i]
        );
    }
}


/*
 * ============================================================
 * Page-fault command
 * ============================================================
 */
static int command_is_pf(
    const char *command
)
{
    if (command == 0)
        return 0;


    return
        command[0] == 'p' &&
        command[1] == 'f' &&
        command[2] == '\n' &&
        command[3] == '\0';
}


/*
 * ============================================================
 * Heap stress state
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
 * Heap stress test
 * ============================================================
 */
static int heap_stress_test(void)
{
    uint32_t seed =
        0x12345678U;


    for (
        uint32_t round = 0;
        round < HEAP_STRESS_ROUNDS;
        ++round
    ) {

        /*
         * Allocate blocks.
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            ++i
        ) {

            seed =
                stress_next(seed);


            uint32_t size =
                8U +
                (seed % 505U);


            stress_sizes[i] =
                size;


            stress_ptrs[i] =
                kmalloc(size);


            if (
                stress_ptrs[i] == 0
            ) {
                return 0;
            }


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
         * Verify blocks.
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

                if (
                    bytes[j] != pattern
                ) {
                    return 0;
                }
            }
        }


        if (
            !kmalloc_validate()
        ) {
            return 0;
        }


        /*
         * Free every second block.
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            i += 2
        ) {

            kfree(
                stress_ptrs[i]
            );

            stress_ptrs[i] =
                0;
        }


        if (
            !kmalloc_validate()
        ) {
            return 0;
        }


        /*
         * Reuse freed blocks.
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


            if (
                stress_ptrs[i] == 0
            ) {
                return 0;
            }


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


            for (
                uint32_t j = 0;
                j < size;
                ++j
            ) {

                if (
                    bytes[j] != pattern
                ) {
                    return 0;
                }
            }
        }


        if (
            !kmalloc_validate()
        ) {
            return 0;
        }


        /*
         * Free everything.
         */
        for (
            uint32_t i = 0;
            i < HEAP_STRESS_SLOTS;
            ++i
        ) {

            kfree(
                stress_ptrs[i]
            );

            stress_ptrs[i] =
                0;
        }


        if (
            !kmalloc_validate()
        ) {
            return 0;
        }
    }


    return 1;
}


/*
 * ============================================================
 * Task A
 * ============================================================
 */
static void task_a_entry(
    void *arg
)
{
    (void)arg;


    for (;;) {

        task_a_runs++;


        serial_write(
            "[task A] running\n"
        );


        console_putc(
            'A'
        );


        if (
            task_a_runs >= 5U
        ) {
            return;
        }


        task_yield();
    }
}


/*
 * ============================================================
 * Task B
 * ============================================================
 */
static void task_b_entry(
    void *arg
)
{
    (void)arg;


    for (;;) {

        task_b_runs++;


        serial_write(
            "[task B] running\n"
        );


        console_putc(
            'B'
        );


        if (
            task_b_runs >= 5U
        ) {
            return;
        }


        task_yield();
    }
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
     * Serial
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
     * VGA
     * ==================================================
     */
    console_init();

    console_set_color(
        VGA_LCYAN,
        VGA_BLACK
    );

    console_write(
        "firstOS v0.0.17\n"
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
     * Multiboot
     * ==================================================
     */
    if (
        magic != MULTIBOOT_MAGIC
    ) {

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
    timer_init(
        100
    );

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
     * PMM
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


    if (
        page_a == 0 ||
        page_b == 0
    ) {

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
     * Kernel heap
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
     * Basic heap allocation test
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
        (char *)kmalloc(
            32
        );

    char *heap_b =
        (char *)kmalloc(
            128
        );

    char *heap_c =
        (char *)kmalloc(
            512
        );


    if (
        heap_a == 0 ||
        heap_b == 0 ||
        heap_c == 0
    ) {

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


    heap_a[0] =
        'A';

    heap_a[1] =
        '\0';

    heap_b[0] =
        'B';

    heap_b[1] =
        '\0';

    heap_c[0] =
        'C';

    heap_c[1] =
        '\0';


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
     * kfree + reuse
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
        kmalloc(
            64
        );


    if (
        heap_d == 0
    ) {

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


    if (
        !heap_stress_test()
    ) {

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
    if (
        !kmalloc_validate()
    ) {

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


    if (
        !paging_init()
    ) {

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

    console_write(
        "[ ok ] page tables initialized\n"
    );


    serial_write(
        "[ ok ] page directory initialized\n"
    );

    serial_write(
        "[ ok ] page tables initialized\n"
    );


    /*
     * Enable paging.
     */
    paging_enable();


    if (
        !paging_is_enabled()
    ) {

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
     * ==================================================
     * Paging statistics
     * ==================================================
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
     * Paging memory test
     * ==================================================
     */
    volatile uint32_t paging_test =
        0x12345678U;


    if (
        paging_test !=
        0x12345678U
    ) {

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


    if (
        paging_test !=
        0xDEADBEEFU
    ) {

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
     * Paging map/unmap subsystem test
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


    uint32_t original_physical =
        0;

    uint32_t original_flags =
        0;


    if (
        !paging_get_page(
            PAGING_TEST_VIRTUAL,
            &original_physical,
            &original_flags
        )
    ) {

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


    uint32_t test_physical =
        pmm_alloc_page();


    if (
        test_physical == 0
    ) {

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


    if (
        !paging_map_page(
            PAGING_TEST_VIRTUAL,
            test_physical,
            PAGE_WRITABLE
        )
    ) {

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


    uint32_t mapped_physical =
        0;

    uint32_t mapped_flags =
        0;


    if (
        !paging_get_page(
            PAGING_TEST_VIRTUAL,
            &mapped_physical,
            &mapped_flags
        )
    ) {

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
        mapped_physical !=
        test_physical
    ) {

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


    volatile uint32_t *test_virtual =
        (volatile uint32_t *)(
            uintptr_t
        )PAGING_TEST_VIRTUAL;


    *test_virtual =
        0xCAFEBABEU;


    if (
        *test_virtual !=
        0xCAFEBABEU
    ) {

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


    if (
        !paging_unmap_page(
            PAGING_TEST_VIRTUAL
        )
    ) {

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


    if (
        paging_get_page(
            PAGING_TEST_VIRTUAL,
            &mapped_physical,
            &mapped_flags
        )
    ) {

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


    if (
        !paging_map_page(
            PAGING_TEST_VIRTUAL,
            original_physical,
            original_flags
        )
    ) {

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
     * VMM
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] initializing vmm\n"
    );

    serial_write(
        "[test] initializing vmm\n"
    );


    if (
        !vmm_init()
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm initialization\n"
        );

        serial_write(
            "[fail] vmm initialization\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] vmm initialized\n"
    );

    serial_write(
        "[ ok ] vmm initialized\n"
    );


    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] vmm page allocation\n"
    );

    serial_write(
        "[test] vmm page allocation\n"
    );


    uint32_t vmm_page =
        vmm_alloc_page(
            PAGE_WRITABLE
        );


    if (
        vmm_page == 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm_alloc_page\n"
        );

        serial_write(
            "[fail] vmm_alloc_page\n"
        );

        halt_cpu();
    }


    volatile uint32_t *vmm_memory =
        (volatile uint32_t *)(
            uintptr_t
        )vmm_page;


    *vmm_memory =
        0xAABBCCDDU;


    if (
        *vmm_memory !=
        0xAABBCCDDU
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm virtual memory access\n"
        );

        serial_write(
            "[fail] vmm virtual memory access\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] vmm virtual memory access\n"
    );

    serial_write(
        "[ ok ] vmm virtual memory access\n"
    );


    uint32_t vmm_physical =
        0;

    uint32_t vmm_flags =
        0;


    if (
        !paging_get_page(
            vmm_page,
            &vmm_physical,
            &vmm_flags
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm page mapping query\n"
        );

        serial_write(
            "[fail] vmm page mapping query\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] vmm mapping query\n"
    );

    serial_write(
        "[ ok ] vmm mapping query\n"
    );


    console_write(
        "[info] vmm used pages:  "
    );

    print_uint32(
        vmm_get_used_pages()
    );

    console_write(
        "\n"
    );


    console_write(
        "[info] vmm free pages:  "
    );

    print_uint32(
        vmm_get_free_pages()
    );

    console_write(
        "\n"
    );


    serial_write(
        "[info] vmm used pages:  "
    );

    serial_write_dec(
        vmm_get_used_pages()
    );

    serial_write(
        "\n"
    );


    serial_write(
        "[info] vmm free pages:  "
    );

    serial_write_dec(
        vmm_get_free_pages()
    );

    serial_write(
        "\n"
    );


    if (
        !vmm_free_page(
            vmm_page
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm_free_page\n"
        );

        serial_write(
            "[fail] vmm_free_page\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] vmm page released\n"
    );

    serial_write(
        "[ ok ] vmm page released\n"
    );


    if (
        vmm_get_used_pages() != 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] vmm page accounting\n"
        );

        serial_write(
            "[fail] vmm page accounting\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] virtual memory manager test passed\n"
    );

    serial_write(
        "[ ok ] virtual memory manager test passed\n"
    );


    /*
     * ==================================================
     * Address Space
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] creating address space\n"
    );

    serial_write(
        "[test] creating address space\n"
    );


    struct address_space *test_space =
        address_space_create();


    if (
        test_space == 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_create\n"
        );

        serial_write(
            "[fail] address_space_create\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] address space created\n"
    );

    serial_write(
        "[ ok ] address space created\n"
    );


    /*
     * Allocate test physical frame.
     */
    uint32_t user_physical =
        pmm_alloc_page();


    if (
        user_physical == 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] user physical page allocation\n"
        );

        serial_write(
            "[fail] user physical page allocation\n"
        );

        halt_cpu();
    }


    /*
     * Map user page.
     */
    if (
        !address_space_map_page(
            test_space,
            ADDRESS_SPACE_TEST_VIRTUAL,
            user_physical,
            PAGE_WRITABLE |
            PAGE_USER
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_map_page\n"
        );

        serial_write(
            "[fail] address_space_map_page\n"
        );

        pmm_free_page(
            user_physical
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] user page mapped\n"
    );

    serial_write(
        "[ ok ] user page mapped\n"
    );


    /*
     * Query mapping.
     */
    uint32_t queried_physical =
        0;

    uint32_t queried_flags =
        0;


    if (
        !address_space_get_page(
            test_space,
            ADDRESS_SPACE_TEST_VIRTUAL,
            &queried_physical,
            &queried_flags
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_get_page\n"
        );

        serial_write(
            "[fail] address_space_get_page\n"
        );

        halt_cpu();
    }


    if (
        queried_physical !=
        user_physical
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address space mapping mismatch\n"
        );

        serial_write(
            "[fail] address space mapping mismatch\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] address space mapping verified\n"
    );

    serial_write(
        "[ ok ] address space mapping verified\n"
    );


    /*
     * Save kernel CR3.
     */
    uint32_t kernel_cr3 =
        address_space_get_current_cr3();


    /*
     * Switch to address space.
     */
    if (
        !address_space_switch(
            test_space
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_switch\n"
        );

        serial_write(
            "[fail] address_space_switch\n"
        );

        halt_cpu();
    }


    if (
        address_space_get_current_cr3() !=
        test_space->page_directory_physical
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] CR3 switch verification\n"
        );

        serial_write(
            "[fail] CR3 switch verification\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] address space switched\n"
    );

    serial_write(
        "[ ok ] address space switched\n"
    );


    /*
     * Access mapped user virtual address from CPL0.
     */
    volatile uint32_t *user_memory =
        (volatile uint32_t *)(
            uintptr_t
        )ADDRESS_SPACE_TEST_VIRTUAL;


    *user_memory =
        0x11223344U;


    if (
        *user_memory !=
        0x11223344U
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address space virtual access\n"
        );

        serial_write(
            "[fail] address space virtual access\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] address space virtual access\n"
    );

    serial_write(
        "[ ok ] address space virtual access\n"
    );


    /*
     * Unmap.
     */
    if (
        !address_space_unmap_page(
            test_space,
            ADDRESS_SPACE_TEST_VIRTUAL
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_unmap_page\n"
        );

        serial_write(
            "[fail] address_space_unmap_page\n"
        );

        halt_cpu();
    }


    console_write(
        "[ ok ] user page unmapped\n"
    );

    serial_write(
        "[ ok ] user page unmapped\n"
    );


    /*
     * Restore kernel CR3.
     */
    if (
        address_space_get_current_cr3() !=
        kernel_cr3
    ) {

        __asm__ volatile (
            "mov %0, %%cr3"
            :
            : "r"(kernel_cr3)
            : "memory"
        );
    }


    if (
        address_space_get_current_cr3() !=
        kernel_cr3
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] kernel CR3 restore\n"
        );

        serial_write(
            "[fail] kernel CR3 restore\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] kernel address space restored\n"
    );

    serial_write(
        "[ ok ] kernel address space restored\n"
    );


    /*
     * Release physical test page.
     */
    pmm_free_page(
        user_physical
    );


    console_write(
        "[ ok ] user physical page released\n"
    );

    serial_write(
        "[ ok ] user physical page released\n"
    );


    /*
     * Destroy address space.
     */
    if (
        !address_space_destroy(
            test_space
        )
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] address_space_destroy\n"
        );

        serial_write(
            "[fail] address_space_destroy\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] address space destroyed\n"
    );

    serial_write(
        "[ ok ] address space destroyed\n"
    );


    console_write(
        "[ ok ] address space subsystem test passed\n"
    );

    serial_write(
        "[ ok ] address space subsystem test passed\n"
    );


    /*
     * ==================================================
     * Task subsystem
     * ==================================================
     *
     * IMPORTANT:
     *
     * Interrupts are still disabled here.
     *
     * We deliberately test cooperative context switching
     * independently from PIT/preemption.
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] initializing task subsystem\n"
    );

    serial_write(
        "[test] initializing task subsystem\n"
    );


    if (
        !task_init()
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] task initialization\n"
        );

        serial_write(
            "[fail] task initialization\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] task subsystem initialized\n"
    );

    serial_write(
        "[ ok ] task subsystem initialized\n"
    );


    /*
     * ==================================================
     * Create task A
     * ==================================================
     */
    uint32_t task_a_id =
        task_create(
            task_a_entry,
            0
        );


    if (
        task_a_id == 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] task A creation\n"
        );

        serial_write(
            "[fail] task A creation\n"
        );

        halt_cpu();
    }


    /*
     * ==================================================
     * Create task B
     * ==================================================
     */
    uint32_t task_b_id =
        task_create(
            task_b_entry,
            0
        );


    if (
        task_b_id == 0
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] task B creation\n"
        );

        serial_write(
            "[fail] task B creation\n"
        );

        halt_cpu();
    }


    console_set_color(
        VGA_LGREEN,
        VGA_BLACK
    );

    console_write(
        "[ ok ] task A created\n"
    );

    console_write(
        "[ ok ] task B created\n"
    );


    serial_write(
        "[ ok ] task A created\n"
    );

    serial_write(
        "[ ok ] task B created\n"
    );


    /*
     * ==================================================
     * Start cooperative scheduler
     * ==================================================
     */
    console_set_color(
        VGA_YELLOW,
        VGA_BLACK
    );

    console_write(
        "[test] starting cooperative scheduler\n"
    );

    serial_write(
        "[test] starting cooperative scheduler\n"
    );


    /*
     * This function transfers control to task A.
     *
     * The task test will eventually halt the CPU after
     * both tasks have returned.
     */
    if (
        !task_start()
    ) {

        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fail] task scheduler start\n"
        );

        serial_write(
            "[fail] task scheduler start\n"
        );

        halt_cpu();
    }


    /*
     * Normally never reached in this prototype because
     * the final DEAD task halts the CPU.
     */
    console_set_color(
        VGA_LRED,
        VGA_BLACK
    );

    console_write(
        "[fail] scheduler returned unexpectedly\n"
    );

    serial_write(
        "[fail] scheduler returned unexpectedly\n"
    );

    halt_cpu();


    /*
     * ==================================================
     * Enable interrupts
     * ==================================================
     *
     * This point will only matter after task scheduling
     * is converted into a preemptive model.
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
     * Command loop
     * ==================================================
     */
    for (;;) {

        int length =
            keyboard_readline(
                command,
                COMMAND_BUFFER_SIZE
            );


        if (
            length > 0
        ) {

            /*
             * Page Fault test command.
             *
             * Type:
             *
             *     pf
             */
            if (
                command_is_pf(
                    command
                )
            ) {

                console_set_color(
                    VGA_YELLOW,
                    VGA_BLACK
                );

                console_write(
                    "\n[test] triggering page fault\n"
                );

                serial_write(
                    "\n[test] triggering page fault\n"
                );


                /*
                 * Intentional page fault.
                 */
                volatile uint32_t *bad =
                    (volatile uint32_t *)(
                        uintptr_t
                    )PAGE_FAULT_TEST_ADDRESS;


                uint32_t value =
                    *bad;


                (void)value;
            }


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