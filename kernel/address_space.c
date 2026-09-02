#include <stdint.h>

#include "address_space.h"
#include "paging.h"
#include "pmm.h"
#include "kmalloc.h"


/*
 * ============================================================
 * Page-directory constants
 * ============================================================
 */
#define PAGE_DIRECTORY_INDEX(address) \
    (((address) >> 22) & 0x3FFU)

#define PAGE_TABLE_INDEX(address) \
    (((address) >> 12) & 0x3FFU)


/*
 * Page-directory/table address mask.
 */
#define PAGE_ADDRESS_MASK 0xFFFFF000U


/*
 * ============================================================
 * Read current CR3
 * ============================================================
 */
static uint32_t read_cr3(void)
{
    uint32_t value;

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(value)
        :
        : "memory"
    );

    return value;
}


/*
 * ============================================================
 * Write CR3
 * ============================================================
 */
static void write_cr3(
    uint32_t value
)
{
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(value)
        : "memory"
    );
}


/*
 * ============================================================
 * TLB invalidate
 * ============================================================
 */
static void tlb_invalidate(
    uint32_t virtual_address
)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"(virtual_address)
        : "memory"
    );
}


/*
 * ============================================================
 * Clear one physical page
 * ============================================================
 *
 * Current firstOS bootstrap paging identity-maps low
 * physical memory, which lets us access page-table memory
 * directly.
 */
static void clear_page(
    uint32_t physical_address
)
{
    uint32_t *page =
        (uint32_t *)(uintptr_t)physical_address;


    for (
        uint32_t i = 0;
        i < PAGE_TABLE_ENTRIES;
        ++i
    ) {
        page[i] = 0;
    }
}


/*
 * ============================================================
 * Get page-table pointer from PDE
 * ============================================================
 */
static uint32_t *page_table_from_pde(
    uint32_t pde
)
{
    uint32_t physical_address =
        pde & PAGE_ADDRESS_MASK;


    return
        (uint32_t *)(uintptr_t)
        physical_address;
}


/*
 * ============================================================
 * Check page alignment
 * ============================================================
 */
static int page_aligned(
    uint32_t address
)
{
    return
        (address &
         (PAGE_SIZE - 1U)) == 0;
}


/*
 * ============================================================
 * Check user virtual address
 * ============================================================
 */
static int valid_user_address(
    uint32_t virtual_address
)
{
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

    return page_aligned(
        virtual_address
    );
}


/*
 * ============================================================
 * Create address space
 * ============================================================
 */
struct address_space *address_space_create(void)
{
    struct address_space *space;


    /*
     * Allocate metadata.
     */
    space =
        (struct address_space *)kmalloc(
            sizeof(struct address_space)
        );


    if (space == 0)
        return 0;


    /*
     * Allocate physical page for page directory.
     */
    uint32_t directory_physical =
        pmm_alloc_page();


    if (directory_physical == 0) {

        kfree(space);

        return 0;
    }


    /*
     * Clear new page directory.
     */
    clear_page(
        directory_physical
    );


    space->page_directory_physical =
        directory_physical;

    space->page_directory =
        (uint32_t *)(uintptr_t)
        directory_physical;

    space->owned_page_tables =
        0;

    space->mapped_pages =
        0;

    space->initialized =
        1;


    /*
     * ==================================================
     * Share kernel identity mappings.
     * ==================================================
     *
     * firstOS currently maps:
     *
     *     0 -> 128 MiB
     *
     * with 32 page tables.
     *
     * These page tables are shared between all address
     * spaces.
     */
    uint32_t current_cr3 =
        read_cr3();


    uint32_t *current_directory =
        (uint32_t *)(uintptr_t)(
            current_cr3 &
            PAGE_ADDRESS_MASK
        );


    for (
        uint32_t i = 0;
        i < ADDRESS_SPACE_KERNEL_PDE_COUNT;
        ++i
    ) {

        space->page_directory[i] =
            current_directory[i];
    }


    return space;
}


/*
 * ============================================================
 * Switch address space
 * ============================================================
 */
int address_space_switch(
    struct address_space *space
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    if (space->page_directory == 0)
        return 0;


    write_cr3(
        space->page_directory_physical
    );


    return 1;
}


/*
 * ============================================================
 * Current CR3
 * ============================================================
 */
uint32_t address_space_get_current_cr3(void)
{
    return
        read_cr3();
}


/*
 * ============================================================
 * Map page
 * ============================================================
 */
int address_space_map_page(
    struct address_space *space,
    uint32_t virtual_address,
    uint32_t physical_address,
    uint32_t flags
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    /*
     * Only user region is managed here.
     */
    if (
        !valid_user_address(
            virtual_address
        )
    ) {
        return 0;
    }

    if (
        !page_aligned(
            physical_address
        )
    ) {
        return 0;
    }


    /*
     * User mappings must explicitly contain USER.
     */
    if (
        (flags & PAGE_USER) == 0
    ) {
        return 0;
    }


    uint32_t directory_index =
        PAGE_DIRECTORY_INDEX(
            virtual_address
        );


    uint32_t table_index =
        PAGE_TABLE_INDEX(
            virtual_address
        );


    uint32_t pde =
        space->page_directory[
            directory_index
        ];


    uint32_t *page_table;


    /*
     * ==================================================
     * Page table creation
     * ==================================================
     */
    if (
        (pde & PAGE_PRESENT) == 0
    ) {

        uint32_t table_physical =
            pmm_alloc_page();


        if (table_physical == 0)
            return 0;


        clear_page(
            table_physical
        );


        /*
         * User page table.
         */
        space->page_directory[
            directory_index
        ] =
            table_physical |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER;


        space->owned_page_tables++;


        pde =
            space->page_directory[
                directory_index
            ];
    }


    /*
     * Convert PDE into table pointer.
     */
    page_table =
        page_table_from_pde(
            pde
        );


    /*
     * Refuse overwrite.
     */
    if (
        page_table[
            table_index
        ] & PAGE_PRESENT
    ) {
        return 0;
    }


    /*
     * Preserve supported flags only.
     */
    flags &=
        PAGE_PRESENT |
        PAGE_WRITABLE |
        PAGE_USER;


    flags |=
        PAGE_PRESENT;


    /*
     * Install mapping.
     */
    page_table[
        table_index
    ] =
        physical_address |
        flags;


    /*
     * Update statistics.
     */
    space->mapped_pages++;


    /*
     * If this address space is currently active,
     * invalidate stale TLB state.
     */
    if (
        read_cr3() ==
        space->page_directory_physical
    ) {

        tlb_invalidate(
            virtual_address
        );
    }


    return 1;
}


/*
 * ============================================================
 * Unmap page
 * ============================================================
 */
int address_space_unmap_page(
    struct address_space *space,
    uint32_t virtual_address
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    if (
        !valid_user_address(
            virtual_address
        )
    ) {
        return 0;
    }


    uint32_t directory_index =
        PAGE_DIRECTORY_INDEX(
            virtual_address
        );


    uint32_t table_index =
        PAGE_TABLE_INDEX(
            virtual_address
        );


    uint32_t pde =
        space->page_directory[
            directory_index
        ];


    /*
     * No page table.
     */
    if (
        (pde & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    uint32_t *page_table =
        page_table_from_pde(
            pde
        );


    /*
     * Page is not mapped.
     */
    if (
        (page_table[
            table_index
        ] & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    /*
     * Remove PTE.
     */
    page_table[
        table_index
    ] = 0;


    if (
        space->mapped_pages > 0
    ) {
        space->mapped_pages--;
    }


    /*
     * Flush TLB if active.
     */
    if (
        read_cr3() ==
        space->page_directory_physical
    ) {

        tlb_invalidate(
            virtual_address
        );
    }


    return 1;
}


/*
 * ============================================================
 * Query mapping
 * ============================================================
 */
int address_space_get_page(
    struct address_space *space,
    uint32_t virtual_address,
    uint32_t *physical_address,
    uint32_t *flags
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    if (physical_address == 0)
        return 0;

    if (
        !valid_user_address(
            virtual_address
        )
    ) {
        return 0;
    }


    uint32_t directory_index =
        PAGE_DIRECTORY_INDEX(
            virtual_address
        );


    uint32_t table_index =
        PAGE_TABLE_INDEX(
            virtual_address
        );


    uint32_t pde =
        space->page_directory[
            directory_index
        ];


    if (
        (pde & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    uint32_t *page_table =
        page_table_from_pde(
            pde
        );


    uint32_t pte =
        page_table[
            table_index
        ];


    if (
        (pte & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    *physical_address =
        pte &
        PAGE_ADDRESS_MASK;


    if (flags != 0) {

        *flags =
            pte &
            0x00000FFFU;
    }


    return 1;
}


/*
 * ============================================================
 * Destroy address space
 * ============================================================
 */
int address_space_destroy(
    struct address_space *space
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;


    /*
     * Never destroy the active address space.
     */
    if (
        read_cr3() ==
        space->page_directory_physical
    ) {
        return 0;
    }


    /*
     * ==================================================
     * Free user-owned page tables.
     * ==================================================
     *
     * Kernel PDEs:
     *
     *     0 .. 31
     *
     * are shared and must NOT be freed.
     */
    for (
        uint32_t directory_index =
            ADDRESS_SPACE_KERNEL_PDE_COUNT;
        directory_index <
            PAGE_DIRECTORY_ENTRIES;
        ++directory_index
    ) {

        uint32_t pde =
            space->page_directory[
                directory_index
            ];


        if (
            (pde & PAGE_PRESENT) == 0
        ) {
            continue;
        }


        uint32_t table_physical =
            pde &
            PAGE_ADDRESS_MASK;


        /*
         * This page table belongs to this address
         * space, so it may be returned to PMM.
         *
         * IMPORTANT:
         *
         * PTE physical frames are deliberately not
         * freed here.
         */
        pmm_free_page(
            table_physical
        );
    }


    /*
     * Return page-directory page.
     */
    pmm_free_page(
        space->page_directory_physical
    );


    space->initialized =
        0;


    /*
     * Free metadata.
     */
    kfree(
        space
    );


    return 1;
}


/*
 * ============================================================
 * Statistics
 * ============================================================
 */
uint32_t address_space_get_mapped_pages(
    const struct address_space *space
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    return
        space->mapped_pages;
}


uint32_t address_space_get_owned_page_tables(
    const struct address_space *space
)
{
    if (space == 0)
        return 0;

    if (!space->initialized)
        return 0;

    return
        space->owned_page_tables;
}