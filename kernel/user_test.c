#include "initrd.h"
#include <stdint.h>

#include "user_test.h"
#include "task.h"
#include "address_space.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "console.h"

#define USER_CODE_A  0x40000000U
#define USER_CODE_B  0x40002000U

#define USER_STACK_A 0x40001000U
#define USER_STACK_B 0x40003000U

#define USER_STACK_TOP_A \
    (USER_STACK_A + PAGE_SIZE - 16U)

#define USER_STACK_TOP_B \
    (USER_STACK_B + PAGE_SIZE - 16U)

/*
 * User task A:
 *
 *     jmp $
 *
 * Infinite CPU loop.
 *
 * There is intentionally NO syscall and NO yield.
 *
 * PIT IRQ0 must preempt this task.
 */
static const char user_message_a[] =
    "Hello from Ring 3 via SYS_WRITE!\n";

/*
 * User task A:
 *
 *     mov eax, SYS_WRITE
 *     mov ebx, USER_STACK_A + 0x100
 *     mov ecx, sizeof(user_message_a) - 1
 *     int 0x80
 *     jmp $
 */
static const uint8_t user_code_a[] = {
    /* mov eax, 1 */
    0xB8, 0x01, 0x00, 0x00, 0x00,

    /* mov ebx, 0x40001100 */
    0xBB, 0x00, 0x11, 0x00, 0x40,

    /* mov ecx, message length */
    0xB9,
    (uint8_t)((sizeof(user_message_a) - 1U) & 0xFFU),
    (uint8_t)(((sizeof(user_message_a) - 1U) >> 8) & 0xFFU),
    (uint8_t)(((sizeof(user_message_a) - 1U) >> 16) & 0xFFU),
    (uint8_t)(((sizeof(user_message_a) - 1U) >> 24) & 0xFFU),

    /* int 0x80 */
    0xCD, 0x80,

    /*
     * mov eax, SYS_EXIT
     */
    0xB8, 0x03, 0x00, 0x00, 0x00,

    /*
     * int 0x80
     */
    0xCD, 0x80,

    /*
     * Should never be reached after SYS_EXIT.
     */
    0xEB, 0xFE
};

/*
 * User task B:
 *
 *     mov eax, SYS_YIELD
 *     int 0x80
 *     jmp $
 */
static const uint8_t user_code_b[] = {
    /*
     * SYS_IPC_RECV
     *
     * Receive into:
     *
     *     0x40003100
     *
     * Capacity:
     *
     *     64 bytes
     */
    0xB8, 0x06, 0x00, 0x00, 0x00,       /* mov eax, 6 */
    0xBB, 0x00, 0x31, 0x00, 0x40,       /* mov ebx, 0x40003100 */
    0xB9, 0x40, 0x00, 0x00, 0x00,       /* mov ecx, 64 */
    0xCD, 0x80,                          /* int 0x80 */

    /*
     * Receive returns:
     *
     *     EAX >= 0 : bytes received
     *     EAX = -1  : no message yet
     *
     * Retry with SYS_YIELD when queue is empty.
     */
    0x85, 0xC0,                          /* test eax, eax */
    0x78, 0xEA,                          /* js retry */

    /*
     * Write exactly the number of received bytes.
     */
    0x89, 0xC1,                          /* mov ecx, eax */
    0xB8, 0x01, 0x00, 0x00, 0x00,       /* mov eax, 1 */
    0xBB, 0x00, 0x31, 0x00, 0x40,       /* mov ebx, 0x40003100 */
    0xCD, 0x80,                          /* int 0x80 */

    /*
     * Exit.
     */
    0xB8, 0x03, 0x00, 0x00, 0x00,       /* mov eax, 3 */
    0xCD, 0x80,                          /* int 0x80 */

    /*
     * retry:
     *
     * SYS_YIELD
     */
    0xB8, 0x02, 0x00, 0x00, 0x00,       /* mov eax, 2 */
    0xCD, 0x80,                          /* int 0x80 */
    0xEB, 0xD6                           /* jmp retry */
};

static int setup_user_task(
    uint32_t task_id,
    uint32_t code_va,
    uint32_t stack_va,
    const uint8_t *code,
    uint32_t code_size,
    uint32_t stack_top,
    char task_name
)
{
    struct task *task =
        task_get(task_id);

    if (task == 0)
        return 0;

    uint32_t code_page =
        pmm_alloc_page();

    if (code_page == 0)
        return 0;

    uint32_t stack_page =
        pmm_alloc_page();

    if (stack_page == 0) {
        pmm_free_page(
            code_page
        );

        return 0;
    }

    if (
        !address_space_map_page(
            task->address_space,
            code_va,
            code_page,
            /*
             * Code is written by the kernel through the
             * identity-mapped physical address below, never
             * through this user virtual mapping, so the user
             * mapping does not need PAGE_WRITABLE. Keeping the
             * code page read+execute only (no write) prevents a
             * compromised Ring 3 task from self-modifying its
             * own instructions (W^X).
             */
            PAGE_PRESENT |
            PAGE_USER
        )
    ) {
        pmm_free_page(
            code_page
        );

        pmm_free_page(
            stack_page
        );

        return 0;
    }

    if (
        !address_space_map_page(
            task->address_space,
            stack_va,
            stack_page,
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER
        )
    ) {
        /*
         * The code mapping succeeded, so remove it before
         * returning both physical pages to the PMM.
         */
        address_space_unmap_page(
            task->address_space,
            code_va
        );

        pmm_free_page(
            code_page
        );

        pmm_free_page(
            stack_page
        );

        return 0;
    }

    /*
     * The bootstrap kernel mapping identity-maps physical memory.
     */
    uint8_t *dst =
        (uint8_t *)(uintptr_t)code_page;

    for (
        uint32_t i = 0;
        i < code_size;
        ++i
    ) {
        dst[i] =
            code[i];
    }


    /*
     * Put the SYS_WRITE message into the mapped user
     * stack page. The message is accessed later through
     * its user virtual address.
     */
    if (task_name == 'A') {

        uint8_t *user_page =
            (uint8_t *)(uintptr_t)stack_page;

        for (
            uint32_t i = 0;
            i < sizeof(user_message_a) - 1U;
            ++i
        ) {
            user_page[0x100U + i] =
                (uint8_t)user_message_a[i];
        }
    }


    task->user_code_physical =
        code_page;

    task->user_code_virtual =
        code_va;

    task->user_code_size =
        PAGE_SIZE;

    task->user_stack_physical =
        stack_page;

    task->user_stack_virtual =
        stack_va;

    task->user_stack_size =
        PAGE_SIZE;

    /*
     * ========================================================
     * Ring 3 code mapping diagnostic
     * ========================================================
     */
    {
        uint32_t mapped_physical = 0;
        uint32_t mapped_flags = 0;

        if (
            !address_space_get_page(
                task->address_space,
                code_va,
                &mapped_physical,
                &mapped_flags
            )
        ) {
            serial_write(
                "[fail] Ring 3 code mapping query\n"
            );

            return 0;
        }

        serial_write(
            "[diag] Ring 3 code mapping present\n"
        );

        serial_write(
            "[diag] code physical: 0x"
        );

        serial_write_hex32(
            mapped_physical
        );

        serial_write(
            "\n[diag] code flags: 0x"
        );

        serial_write_hex32(
            mapped_flags
        );

        serial_write(
            "\n"
        );

        serial_write(
            "[diag] code bytes: "
        );

        {
            const uint8_t *code_ptr =
                (const uint8_t *)(uintptr_t)code_page;

            for (uint32_t i = 0; i < 16U; ++i) {
                static const char hex[] =
                    "0123456789ABCDEF";

                char out[3];

                out[0] =
                    hex[(code_ptr[i] >> 4) & 0x0FU];

                out[1] =
                    hex[code_ptr[i] & 0x0FU];

                out[2] =
                    '\0';

                serial_write(out);

                if (i != 15U)
                    serial_write(" ");
            }
        }

        serial_write(
            "\n"
        );
    }

    serial_write(
        "[ ok ] created Ring 3 task "
    );

    if (task_name == 'A') {
        serial_write("A\n");
    } else {
        serial_write("B\n");
    }

    /*
     * Ensure these values remain the exact user context.
     */
    task->user_entry =
        code_va;

    task->user_esp =
        stack_top;

    task->state =
        TASK_READY;

    return 1;
}

int user_test_init(void)
{
    serial_write(
        "[test] creating Ring 3 tasks\n"
    );

    console_write(
        "[test] creating Ring 3 tasks\n"
    );

    uint32_t task_a =
        task_create_user(
            USER_CODE_A,
            USER_STACK_TOP_A
        );

    if (task_a == 0) {
        serial_write(
            "[fail] Ring 3 task A creation\n"
        );

        return 0;
    }

    uint32_t task_b =
        task_create_user(
            USER_CODE_B,
            USER_STACK_TOP_B
        );

    if (task_b == 0) {
        serial_write(
            "[fail] Ring 3 task B creation\n"
        );

        return 0;
    }

    uint32_t mod_start = initrd_get_start();
    uint32_t mod_size = initrd_get_size();

    const uint8_t *prog_code = user_code_a;
    uint32_t prog_size = sizeof(user_code_a);

    if (mod_size > 0) {
        prog_code = (const uint8_t *)(uintptr_t)mod_start;
        prog_size = (mod_size > PAGE_SIZE) ? PAGE_SIZE : mod_size;
    }

    if (
        !setup_user_task(
            task_a,
            USER_CODE_A,
            USER_STACK_A,
            prog_code,
            prog_size,
            USER_STACK_TOP_A,
            'A'
        )
    ) {
        serial_write(
            "[fail] Ring 3 task A address space\n"
        );

        return 0;
    }

    if (
        !setup_user_task(
            task_b,
            USER_CODE_B,
            USER_STACK_B,
            user_code_b,
            sizeof(user_code_b),
            USER_STACK_TOP_B,
            'B'
        )
    ) {
        serial_write(
            "[fail] Ring 3 task B address space\n"
        );

        return 0;
    }

    serial_write(
        "[ ok ] Ring 3 task A ready\n"
    );

    serial_write(
        "[ ok ] Ring 3 task B ready\n"
    );

    return 1;
}

void user_test_start(void)
{
    /*
     * The actual transition is now performed by task_start().
     *
     * The scheduler will eventually select the Ring 3 tasks
     * and their saved iret frames will enter CPL3.
     */
    serial_write(
        "[test] Ring 3 tasks ready for scheduler\n"
    );

    console_write(
        "[test] Ring 3 tasks ready for scheduler\n"
    );
}