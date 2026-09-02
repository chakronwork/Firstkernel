#include <stdint.h>

#include "user_test.h"
#include "address_space.h"
#include "pmm.h"
#include "paging.h"
#include "console.h"
#include "serial.h"
#include "idt.h"

#define USER_TEST_CODE  0x40000000U
#define USER_TEST_STACK 0x40001000U

#define USER_TEST_CS 0x1BU
#define USER_TEST_DS 0x23U

extern void ring3_enter(
    uint32_t user_eip,
    uint32_t user_esp
);

/*
 * User program:
 *
 *     int 0x30
 *     jmp $-2
 *
 * CD 30
 * EB FC
 *
 * Once inside Ring 3, this repeatedly enters
 * the kernel through vector 0x30.
 */
static const uint8_t user_code[] = {
    0xCD, 0x30,
    0xEB, 0xFC
};

static struct address_space *test_space;
static uint32_t code_page;
static uint32_t stack_page;

int user_test_init(void)
{
    serial_write(
        "[test] preparing Ring 3 context\n"
    );

    test_space =
        address_space_create();

    if (test_space == 0) {

        serial_write(
            "[fail] Ring 3 address space create\n"
        );

        return 0;
    }

    code_page =
        pmm_alloc_page();

    stack_page =
        pmm_alloc_page();

    if (
        code_page == 0 ||
        stack_page == 0
    ) {

        if (code_page != 0)
            pmm_free_page(code_page);

        if (stack_page != 0)
            pmm_free_page(stack_page);

        address_space_destroy(
            test_space
        );

        test_space = 0;

        serial_write(
            "[fail] Ring 3 page allocation\n"
        );

        return 0;
    }

    /*
     * User code mapping.
     */
    if (
        !address_space_map_page(
            test_space,
            USER_TEST_CODE,
            code_page,
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER
        )
    ) {

        pmm_free_page(code_page);
        pmm_free_page(stack_page);

        address_space_destroy(
            test_space
        );

        test_space = 0;

        serial_write(
            "[fail] Ring 3 code mapping\n"
        );

        return 0;
    }

    /*
     * User stack mapping.
     */
    if (
        !address_space_map_page(
            test_space,
            USER_TEST_STACK,
            stack_page,
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER
        )
    ) {

        address_space_unmap_page(
            test_space,
            USER_TEST_CODE
        );

        pmm_free_page(code_page);
        pmm_free_page(stack_page);

        address_space_destroy(
            test_space
        );

        test_space = 0;

        serial_write(
            "[fail] Ring 3 stack mapping\n"
        );

        return 0;
    }

    /*
     * Bootstrap mapping is identity mapped,
     * so write directly to the physical frame.
     */
    uint8_t *code =
        (uint8_t *)(uintptr_t)code_page;

    for (
        uint32_t i = 0;
        i < sizeof(user_code);
        ++i
    ) {
        code[i] = user_code[i];
    }

    if (
        !address_space_switch(
            test_space
        )
    ) {

        serial_write(
            "[fail] Ring 3 CR3 switch\n"
        );

        return 0;
    }

    serial_write(
        "[ ok ] Ring 3 address space active\n"
    );

    console_write(
        "[ ok ] Ring 3 address space active\n"
    );

    return 1;
}

void user_test_start(void)
{
    uint32_t user_esp =
        USER_TEST_STACK +
        PAGE_SIZE -
        16U;

    serial_write(
        "[test] entering Ring 3\n"
    );

    console_write(
        "[test] entering Ring 3\n"
    );

    /*
     * Enable isolated Ring 3 interrupt path.
     */
    idt_set_user_test_active(
        1
    );

    ring3_enter(
        USER_TEST_CODE,
        user_esp
    );

    /*
     * Should never return.
     */
    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}
