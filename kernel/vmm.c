#include <stdint.h>

#include "vmm.h"
#include "paging.h"
#include "pmm.h"


/*
 * ============================================================
 * Virtual page bitmap
 * ============================================================
 *
 * Bit:
 *
 *     0 = free
 *     1 = allocated
 */
static uint32_t vmm_bitmap[
    VMM_BITMAP_WORDS
];


/*
 * Number of pages currently owned by VMM.
 */
static uint32_t used_pages = 0;


/*
 * ============================================================
 * Bitmap helpers
 * ============================================================
 */
static int bitmap_test(
    uint32_t index
)
{
    uint32_t word;
    uint32_t bit;

    word =
        index / 32U;

    bit =
        index % 32U;

    return
        (vmm_bitmap[word] &
         (1U << bit)) != 0;
}


static void bitmap_set(
    uint32_t index
)
{
    uint32_t word;
    uint32_t bit;

    word =
        index / 32U;

    bit =
        index % 32U;

    vmm_bitmap[word] |=
        (1U << bit);
}


static void bitmap_clear(
    uint32_t index
)
{
    uint32_t word;
    uint32_t bit;

    word =
        index / 32U;

    bit =
        index % 32U;

    vmm_bitmap[word] &=
        ~(1U << bit);
}


/*
 * ============================================================
 * Convert virtual address to VMM page index.
 * ============================================================
 */
static uint32_t virtual_to_index(
    uint32_t virtual_address
)
{
    return
        (
            virtual_address -
            VMM_START_ADDRESS
        ) / PAGE_SIZE;
}


/*
 * ============================================================
 * Convert VMM page index to virtual address.
 * ============================================================
 */
static uint32_t index_to_virtual(
    uint32_t index
)
{
    return
        VMM_START_ADDRESS +
        (index * PAGE_SIZE);
}


/*
 * ============================================================
 * Check virtual address range.
 * ============================================================
 */
static int valid_virtual_address(
    uint32_t virtual_address
)
{
    if (
        virtual_address <
        VMM_START_ADDRESS
    ) {
        return 0;
    }

    if (
        virtual_address >=
        VMM_END_ADDRESS
    ) {
        return 0;
    }

    if (
        (virtual_address &
         (PAGE_SIZE - 1U)) != 0
    ) {
        return 0;
    }

    return 1;
}


/*
 * ============================================================
 * Find contiguous free pages
 * ============================================================
 *
 * Returns:
 *
 *     page index
 *     VMM_MAX_PAGES on failure
 */
static uint32_t find_free_range(
    uint32_t count
)
{
    if (count == 0)
        return VMM_MAX_PAGES;

    if (count > VMM_MAX_PAGES)
        return VMM_MAX_PAGES;


    uint32_t run_start = 0;
    uint32_t run_length = 0;


    for (
        uint32_t i = 0;
        i < VMM_MAX_PAGES;
        ++i
    ) {

        if (!bitmap_test(i)) {

            if (run_length == 0)
                run_start = i;

            run_length++;

            if (run_length == count)
                return run_start;

        } else {

            run_length = 0;
        }
    }


    return VMM_MAX_PAGES;
}


/*
 * ============================================================
 * Initialization
 * ============================================================
 */
int vmm_init(void)
{
    for (
        uint32_t i = 0;
        i < VMM_BITMAP_WORDS;
        ++i
    ) {
        vmm_bitmap[i] = 0;
    }

    used_pages = 0;

    return 1;
}


/*
 * ============================================================
 * Allocate contiguous virtual memory
 * ============================================================
 */
uint32_t vmm_alloc_pages(
    uint32_t count,
    uint32_t flags
)
{
    uint32_t start_index;

    if (count == 0)
        return 0;


    start_index =
        find_free_range(count);


    if (
        start_index >=
        VMM_MAX_PAGES
    ) {
        return 0;
    }


    /*
     * ==================================================
     * Allocate and map every page.
     * ==================================================
     */
    for (
        uint32_t i = 0;
        i < count;
        ++i
    ) {

        uint32_t index =
            start_index + i;

        uint32_t virtual_address =
            index_to_virtual(index);


        /*
         * Allocate physical frame.
         */
        uint32_t physical_address =
            pmm_alloc_page();


        if (physical_address == 0) {

            /*
             * Roll back every page already
             * allocated in this operation.
             */
            for (
                uint32_t rollback = 0;
                rollback < i;
                ++rollback
            ) {

                uint32_t rollback_index =
                    start_index + rollback;

                uint32_t rollback_virtual =
                    index_to_virtual(
                        rollback_index
                    );

                uint32_t rollback_physical = 0;

                /*
                 * Read physical mapping.
                 */
                if (
                    paging_get_page(
                        rollback_virtual,
                        &rollback_physical,
                        0
                    )
                ) {

                    paging_unmap_page(
                        rollback_virtual
                    );

                    pmm_free_page(
                        rollback_physical
                    );
                }

                bitmap_clear(
                    rollback_index
                );

                used_pages--;
            }

            return 0;
        }


        /*
         * ==================================================
         * Install page mapping.
         * ==================================================
         */
        if (
            !paging_map_page(
                virtual_address,
                physical_address,
                flags
            )
        ) {

            /*
             * Mapping failed.
             *
             * Return frame to PMM.
             */
            pmm_free_page(
                physical_address
            );


            /*
             * Roll back previous pages.
             */
            for (
                uint32_t rollback = 0;
                rollback < i;
                ++rollback
            ) {

                uint32_t rollback_index =
                    start_index + rollback;

                uint32_t rollback_virtual =
                    index_to_virtual(
                        rollback_index
                    );

                uint32_t rollback_physical = 0;


                if (
                    paging_get_page(
                        rollback_virtual,
                        &rollback_physical,
                        0
                    )
                ) {

                    paging_unmap_page(
                        rollback_virtual
                    );

                    pmm_free_page(
                        rollback_physical
                    );
                }


                bitmap_clear(
                    rollback_index
                );

                used_pages--;
            }

            return 0;
        }


        /*
         * Mark virtual page allocated.
         */
        bitmap_set(index);

        used_pages++;
    }


    return
        index_to_virtual(
            start_index
        );
}


/*
 * ============================================================
 * Allocate one page
 * ============================================================
 */
uint32_t vmm_alloc_page(
    uint32_t flags
)
{
    return
        vmm_alloc_pages(
            1,
            flags
        );
}


/*
 * ============================================================
 * Free multiple pages
 * ============================================================
 */
int vmm_free_pages(
    uint32_t virtual_address,
    uint32_t count
)
{
    if (count == 0)
        return 0;


    if (
        !valid_virtual_address(
            virtual_address
        )
    ) {
        return 0;
    }


    uint32_t start_index =
        virtual_to_index(
            virtual_address
        );


    /*
     * Check range.
     */
    if (
        start_index >=
        VMM_MAX_PAGES
    ) {
        return 0;
    }


    if (
        count >
        (VMM_MAX_PAGES - start_index)
    ) {
        return 0;
    }


    /*
     * ==================================================
     * Validate ownership first.
     * ==================================================
     *
     * We do this before changing anything so a partial
     * free cannot occur.
     */
    for (
        uint32_t i = 0;
        i < count;
        ++i
    ) {

        uint32_t index =
            start_index + i;


        if (!bitmap_test(index))
            return 0;
    }


    /*
     * ==================================================
     * Release pages.
     * ==================================================
     */
    for (
        uint32_t i = 0;
        i < count;
        ++i
    ) {

        uint32_t index =
            start_index + i;

        uint32_t page_virtual =
            index_to_virtual(index);

        uint32_t physical_address = 0;


        /*
         * Find backing physical frame.
         */
        if (
            !paging_get_page(
                page_virtual,
                &physical_address,
                0
            )
        ) {

            return 0;
        }


        /*
         * Remove virtual mapping.
         */
        if (
            !paging_unmap_page(
                page_virtual
            )
        ) {

            return 0;
        }


        /*
         * Return physical frame.
         */
        pmm_free_page(
            physical_address
        );


        /*
         * Mark virtual page free.
         */
        bitmap_clear(
            index
        );

        used_pages--;
    }


    return 1;
}


/*
 * ============================================================
 * Free one page
 * ============================================================
 */
int vmm_free_page(
    uint32_t virtual_address
)
{
    return
        vmm_free_pages(
            virtual_address,
            1
        );
}


/*
 * ============================================================
 * Statistics
 * ============================================================
 */
uint32_t vmm_get_total_pages(void)
{
    return VMM_MAX_PAGES;
}


uint32_t vmm_get_used_pages(void)
{
    return used_pages;
}


uint32_t vmm_get_free_pages(void)
{
    return
        VMM_MAX_PAGES -
        used_pages;
}