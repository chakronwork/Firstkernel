#include <stdint.h>

#include "paging.h"
#include "pmm.h"


/*
 * ============================================================
 * Bootstrap page directory
 * ============================================================
 */
static uint32_t page_directory[
    PAGE_DIRECTORY_ENTRIES
] __attribute__((aligned(PAGE_SIZE)));


/*
 * ============================================================
 * Bootstrap page tables
 * ============================================================
 *
 * 32 × 4 KiB = 128 KiB
 *
 * Enough for 128 MiB of identity mapping.
 */
static uint32_t page_tables[
    PAGING_MAX_PAGE_TABLES
][PAGE_TABLE_ENTRIES]
__attribute__((aligned(PAGE_SIZE)));


/*
 * Number of pages mapped during bootstrap.
 */
static uint32_t mapped_pages = 0;


/*
 * ============================================================
 * Flush one TLB entry.
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
 * Clear a physical page.
 *
 * Before higher-half / separate address spaces exist,
 * physical memory is identity mapped, so the physical
 * address can be accessed directly.
 * ============================================================
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
 * Get page-directory index.
 * ============================================================
 */
static uint32_t page_directory_index(
    uint32_t virtual_address
)
{
    return
        (virtual_address >> 22) &
        0x3FFU;
}


/*
 * ============================================================
 * Get page-table index.
 * ============================================================
 */
static uint32_t page_table_index(
    uint32_t virtual_address
)
{
    return
        (virtual_address >> 12) &
        0x3FFU;
}


/*
 * ============================================================
 * Return page-table virtual pointer from PDE.
 * ============================================================
 *
 * Because we currently use identity mapping:
 *
 *     physical == virtual
 */
static uint32_t *page_table_from_pde(
    uint32_t pde
)
{
    uint32_t physical_address =
        pde & 0xFFFFF000U;


    return
        (uint32_t *)(
            uintptr_t
        )physical_address;
}


/*
 * ============================================================
 * Bootstrap initialization
 * ============================================================
 */
int paging_init(void)
{
    uint32_t total_pages;
    uint32_t table_count;


    total_pages =
        pmm_get_total_pages();


    if (total_pages == 0)
        return 0;


    /*
     * Calculate required page-table count.
     */
    table_count =
        (
            total_pages +
            PAGE_TABLE_ENTRIES -
            1U
        ) /
        PAGE_TABLE_ENTRIES;


    /*
     * Bootstrap implementation supports
     * up to 128 MiB.
     */
    if (table_count >
        PAGING_MAX_PAGE_TABLES)
    {
        return 0;
    }


    /*
     * Clear directory.
     */
    for (
        uint32_t i = 0;
        i < PAGE_DIRECTORY_ENTRIES;
        ++i
    ) {
        page_directory[i] = 0;
    }


    /*
     * Clear bootstrap tables.
     */
    for (
        uint32_t table = 0;
        table < PAGING_MAX_PAGE_TABLES;
        ++table
    ) {

        for (
            uint32_t entry = 0;
            entry < PAGE_TABLE_ENTRIES;
            ++entry
        ) {
            page_tables[table][entry] = 0;
        }
    }


    mapped_pages = 0;


    /*
     * Build identity map.
     */
    for (
        uint32_t table = 0;
        table < table_count;
        ++table
    ) {

        /*
         * Physical address of page table.
         *
         * The BSS containing these tables is already
         * reserved by PMM as part of the kernel image.
         */
        uint32_t table_address =
            (uint32_t)(
                uintptr_t
            )&page_tables[table][0];


        /*
         * PDE:
         *
         * PRESENT
         * WRITABLE
         */
        page_directory[table] =
            table_address |
            PAGE_PRESENT |
            PAGE_WRITABLE;


        /*
         * Fill PTEs.
         */
        for (
            uint32_t entry = 0;
            entry < PAGE_TABLE_ENTRIES;
            ++entry
        ) {

            uint32_t page =
                table *
                PAGE_TABLE_ENTRIES +
                entry;


            if (page >= total_pages)
                break;


            uint32_t physical_address =
                page *
                PAGE_SIZE;


            page_tables[table][entry] =
                physical_address |
                PAGE_PRESENT |
                PAGE_WRITABLE;


            mapped_pages++;
        }
    }


    return 1;
}


/*
 * ============================================================
 * Enable paging
 * ============================================================
 */
void paging_enable(void)
{
    uint32_t page_directory_address =
        (uint32_t)(
            uintptr_t
        )&page_directory[0];


    /*
     * Load CR3.
     */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(page_directory_address)
        : "memory"
    );


    /*
     * Read CR0.
     */
    uint32_t cr0;

    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
        :
        : "memory"
    );


    /*
     * Enable PG.
     */
    cr0 |= 0x80000000U;


    /*
     * Write CR0.
     */
    __asm__ volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );
}


/*
 * ============================================================
 * Paging state
 * ============================================================
 */
int paging_is_enabled(void)
{
    uint32_t cr0;


    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
        :
        : "memory"
    );


    return
        (cr0 & 0x80000000U) != 0;
}


/*
 * ============================================================
 * Bootstrap mapped page count
 * ============================================================
 */
uint32_t paging_get_mapped_pages(void)
{
    return mapped_pages;
}


/*
 * ============================================================
 * Map one virtual page.
 * ============================================================
 */
int paging_map_page(
    uint32_t virtual_address,
    uint32_t physical_address,
    uint32_t flags
)
{
    uint32_t directory_index;
    uint32_t table_index;

    uint32_t pde;

    uint32_t *page_table;

    uint32_t pte;


    /*
     * Both addresses must be page aligned.
     */
    if (
        (virtual_address &
         (PAGE_SIZE - 1U)) != 0
    ) {
        return 0;
    }


    if (
        (physical_address &
         (PAGE_SIZE - 1U)) != 0
    ) {
        return 0;
    }


    directory_index =
        page_directory_index(
            virtual_address
        );


    table_index =
        page_table_index(
            virtual_address
        );


    /*
     * Look at PDE.
     */
    pde =
        page_directory[
            directory_index
        ];


    /*
     * Allocate a new page table if needed.
     */
    if (
        (pde & PAGE_PRESENT) == 0
    ) {

        uint32_t table_physical =
            pmm_alloc_page();


        if (table_physical == 0)
            return 0;


        /*
         * Clear new page table.
         */
        clear_page(
            table_physical
        );


        /*
         * Install PDE.
         */
        page_directory[
            directory_index
        ] =
            table_physical |
            PAGE_PRESENT |
            PAGE_WRITABLE;


        pde =
            page_directory[
                directory_index
            ];
    }


    /*
     * Convert PDE into page-table pointer.
     */
    page_table =
        page_table_from_pde(
            pde
        );


    /*
     * Preserve only supported page flags.
     */
    flags &=
        PAGE_PRESENT |
        PAGE_WRITABLE |
        PAGE_USER;


    /*
     * A mapped page must be present.
     */
    flags |=
        PAGE_PRESENT;


    /*
     * Install PTE.
     */
    pte =
        physical_address |
        flags;


    page_table[
        table_index
    ] =
        pte;


    /*
     * Flush stale TLB entry.
     */
    tlb_invalidate(
        virtual_address
    );


    return 1;
}


/*
 * ============================================================
 * Unmap one virtual page.
 * ============================================================
 */
int paging_unmap_page(
    uint32_t virtual_address
)
{
    uint32_t directory_index;
    uint32_t table_index;

    uint32_t pde;

    uint32_t *page_table;


    /*
     * Must be page aligned.
     */
    if (
        (virtual_address &
         (PAGE_SIZE - 1U)) != 0
    ) {
        return 0;
    }


    directory_index =
        page_directory_index(
            virtual_address
        );


    table_index =
        page_table_index(
            virtual_address
        );


    pde =
        page_directory[
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


    page_table =
        page_table_from_pde(
            pde
        );


    /*
     * Page not mapped.
     */
    if (
        (page_table[
            table_index
        ] & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    /*
     * Clear PTE.
     */
    page_table[
        table_index
    ] = 0;


    /*
     * Flush TLB.
     */
    tlb_invalidate(
        virtual_address
    );


    /*
     * We intentionally keep the page table.
     *
     * Page-table reclamation will be added when
     * ownership is handled by the VM subsystem.
     */
    return 1;
}


/*
 * ============================================================
 * Query mapping.
 * ============================================================
 */
int paging_get_page(
    uint32_t virtual_address,
    uint32_t *physical_address,
    uint32_t *flags
)
{
    uint32_t directory_index;
    uint32_t table_index;

    uint32_t pde;

    uint32_t *page_table;

    uint32_t pte;


    if (
        (virtual_address &
         (PAGE_SIZE - 1U)) != 0
    ) {
        return 0;
    }


    if (physical_address == 0)
        return 0;


    directory_index =
        page_directory_index(
            virtual_address
        );


    table_index =
        page_table_index(
            virtual_address
        );


    pde =
        page_directory[
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


    page_table =
        page_table_from_pde(
            pde
        );


    pte =
        page_table[
            table_index
        ];


    /*
     * Page not present.
     */
    if (
        (pte & PAGE_PRESENT) == 0
    ) {
        return 0;
    }


    /*
     * Return physical frame.
     */
    *physical_address =
        pte &
        0xFFFFF000U;


    /*
     * Return flags.
     */
    if (flags != 0) {

        *flags =
            pte &
            0x00000FFFU;
    }


    return 1;
}