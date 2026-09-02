#include <stdint.h>

#include "uaccess.h"
#include "address_space.h"
#include "paging.h"
#include "task.h"

int user_range_valid(
    uint32_t virtual_address,
    uint32_t length
)
{
    struct task *task =
        task_current();

    if (task == 0)
        return 0;

    if (task->address_space == 0)
        return 0;

    /*
     * A zero-length access is valid as long as the
     * starting address itself is inside user space.
     */
    if (length == 0U) {

        if (
            virtual_address <
            ADDRESS_SPACE_USER_START
        ) {
            return 0;
        }

        if (
            virtual_address >=
            ADDRESS_SPACE_USER_END
        ) {
            return 0;
        }

        return 1;
    }

    /*
     * Detect uint32_t wraparound.
     */
    uint32_t end =
        virtual_address +
        length -
        1U;

    if (end < virtual_address)
        return 0;

    /*
     * Entire range must remain in user virtual space.
     */
    if (
        virtual_address <
        ADDRESS_SPACE_USER_START
    ) {
        return 0;
    }

    if (
        end >=
        ADDRESS_SPACE_USER_END
    ) {
        return 0;
    }

    /*
     * Check every page touched by the range.
     */
    uint32_t page =
        virtual_address &
        ~(PAGE_SIZE - 1U);

    uint32_t last_page =
        end &
        ~(PAGE_SIZE - 1U);

    for (;;) {

        uint32_t physical = 0;
        uint32_t flags = 0;

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
            (flags & PAGE_PRESENT) == 0
        ) {
            return 0;
        }

        if (
            (flags & PAGE_USER) == 0
        ) {
            return 0;
        }

        if (page == last_page)
            break;

        page += PAGE_SIZE;
    }

    return 1;
}


int copy_from_user(
    void *destination,
    uint32_t user_address,
    uint32_t length
)
{
    if (destination == 0)
        return 0;

    if (
        !user_range_valid(
            user_address,
            length
        )
    ) {
        return 0;
    }

    uint8_t *dst =
        (uint8_t *)destination;

    const uint8_t *src =
        (const uint8_t *)(uintptr_t)user_address;

    for (
        uint32_t i = 0;
        i < length;
        ++i
    ) {
        dst[i] = src[i];
    }

    return 1;
}
