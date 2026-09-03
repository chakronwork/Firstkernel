#include <stdint.h>
#include <stddef.h>

#include "uaccess.h"
#include "address_space.h"
#include "paging.h"
#include "task.h"


/*
 * ============================================================
 * Internal user range validation
 * ============================================================
 *
 * Verifies that every page touched by the range:
 *
 *     - belongs to the current task address space
 *     - is present
 *     - is marked as user-accessible
 *
 * This function intentionally keeps the original public
 * uaccess API unchanged.
 */
static int user_range_valid(
    const void *ptr,
    size_t size
)
{
    struct task *task =
        task_current();

    if (task == 0)
        return 0;

    if (task->address_space == 0)
        return 0;


    uintptr_t start =
        (uintptr_t)ptr;


    /*
     * Zero-length access.
     *
     * No memory is dereferenced, so only the address range
     * boundary is checked.
     */
    if (size == 0U) {

        if (
            start <
            USER_SPACE_START
        ) {
            return 0;
        }

        if (
            start >=
            USER_SPACE_END
        ) {
            return 0;
        }

        return 1;
    }


    /*
     * Calculate last byte and reject integer wraparound.
     */
    uintptr_t end =
        start +
        size -
        1U;

    if (end < start)
        return 0;


    /*
     * Entire range must remain inside user space.
     */
    if (
        start <
        USER_SPACE_START
    ) {
        return 0;
    }

    if (
        end >=
        USER_SPACE_END
    ) {
        return 0;
    }


    /*
     * Determine the first and last pages touched.
     */
    uint32_t page =
        (uint32_t)start &
        ~(PAGE_SIZE - 1U);

    uint32_t last_page =
        (uint32_t)end &
        ~(PAGE_SIZE - 1U);


    /*
     * Validate every page.
     */
    for (;;) {

        uint32_t physical =
            0;

        uint32_t flags =
            0;


        if (
            !address_space_get_page(
                task->address_space,
                page,
                &physical,
                &flags
            )
        ) {
            return 0;
        }


        /*
         * The page must be present.
         */
        if (
            (flags & PAGE_PRESENT) ==
            0
        ) {
            return 0;
        }


        /*
         * The page must be user-accessible.
         */
        if (
            (flags & PAGE_USER) ==
            0
        ) {
            return 0;
        }


        if (page == last_page)
            break;


        /*
         * Move to the next page.
         *
         * The maximum valid user address is below
         * UINT32_MAX, so this increment cannot wrap
         * for a valid range.
         */
        page += PAGE_SIZE;
    }


    return 1;
}


/*
 * ============================================================
 * Verify user read access
 * ============================================================
 */
int uaccess_verify_read(
    const void *ptr,
    size_t size
)
{
    return user_range_valid(
        ptr,
        size
    );
}


/*
 * ============================================================
 * Verify user write access
 * ============================================================
 *
 * The public API remains unchanged.
 *
 * In addition to range/mapping checks, verify that every
 * touched page is writable.
 */
int uaccess_verify_write(
    void *ptr,
    size_t size
)
{
    struct task *task =
        task_current();

    if (task == 0)
        return 0;

    if (task->address_space == 0)
        return 0;


    uintptr_t start =
        (uintptr_t)ptr;


    /*
     * Zero-length access.
     */
    if (size == 0U) {

        if (
            start <
            USER_SPACE_START
        ) {
            return 0;
        }

        if (
            start >=
            USER_SPACE_END
        ) {
            return 0;
        }

        return 1;
    }


    /*
     * Calculate last byte and reject wraparound.
     */
    uintptr_t end =
        start +
        size -
        1U;

    if (end < start)
        return 0;


    /*
     * Entire range must remain inside user space.
     */
    if (
        start <
        USER_SPACE_START
    ) {
        return 0;
    }

    if (
        end >=
        USER_SPACE_END
    ) {
        return 0;
    }


    /*
     * Determine first and last pages touched.
     */
    uint32_t page =
        (uint32_t)start &
        ~(PAGE_SIZE - 1U);

    uint32_t last_page =
        (uint32_t)end &
        ~(PAGE_SIZE - 1U);


    /*
     * Validate every page and require write permission.
     */
    for (;;) {

        uint32_t physical =
            0;

        uint32_t flags =
            0;


        if (
            !address_space_get_page(
                task->address_space,
                page,
                &physical,
                &flags
            )
        ) {
            return 0;
        }


        if (
            (flags & PAGE_PRESENT) ==
            0
        ) {
            return 0;
        }


        if (
            (flags & PAGE_USER) ==
            0
        ) {
            return 0;
        }


        /*
         * Do not allow copy_to_user() to write into
         * read-only pages such as the Ring 3 code page.
         */
        if (
            (flags & PAGE_WRITABLE) ==
            0
        ) {
            return 0;
        }


        if (page == last_page)
            break;


        page += PAGE_SIZE;
    }


    return 1;
}


/*
 * ============================================================
 * Copy from user space
 * ============================================================
 */
int copy_from_user(
    void *dst,
    const void *src,
    size_t size
)
{
    if (dst == 0)
        return 0;


    if (
        !uaccess_verify_read(
            src,
            size
        )
    ) {
        return 0;
    }


    uint8_t *destination =
        (uint8_t *)dst;

    const uint8_t *source =
        (const uint8_t *)src;


    for (
        size_t i = 0;
        i < size;
        ++i
    ) {
        destination[i] =
            source[i];
    }


    return 1;
}


/*
 * ============================================================
 * Copy to user space
 * ============================================================
 */
int copy_to_user(
    void *dst,
    const void *src,
    size_t size
)
{
    if (src == 0)
        return 0;


    if (
        !uaccess_verify_write(
            dst,
            size
        )
    ) {
        return 0;
    }


    uint8_t *destination =
        (uint8_t *)dst;

    const uint8_t *source =
        (const uint8_t *)src;


    for (
        size_t i = 0;
        i < size;
        ++i
    ) {
        destination[i] =
            source[i];
    }


    return 1;
}
