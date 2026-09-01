#include <stdint.h>

#include "paging.h"
#include "pmm.h"


/*
 * ============================================================
 * Page directory
 * ============================================================
 *
 * 1024 entries × 4 bytes = 4096 bytes
 *
 * Exactly one page.
 */
static uint32_t page_directory[
    PAGE_DIRECTORY_ENTRIES
] __attribute__((aligned(PAGE_SIZE)));


/*
 * ============================================================
 * Page tables
 * ============================================================
 *
 * 32 tables × 4096 bytes
 *
 * = 128 KiB
 *
 * This is enough to identity-map 128 MiB.
 */
static uint32_t page_tables[
    PAGING_MAX_PAGE_TABLES
][PAGE_TABLE_ENTRIES]
__attribute__((aligned(PAGE_SIZE)));


/*
 * Number of pages actually mapped.
 */
static uint32_t mapped_pages = 0;


/*
 * Paging state.
 */
static int paging_enabled = 0;


/*
 * ============================================================
 * Initialize page directory and page tables.
 * ============================================================
 */
int paging_init(void)
{
    uint32_t total_pages;
    uint32_t table_count;


    /*
     * Get physical memory known by PMM.
     */
    total_pages =
        pmm_get_total_pages();


    /*
     * No physical memory.
     */
    if (total_pages == 0)
        return 0;


    /*
     * Calculate number of 1024-entry page tables.
     *
     * Example:
     *
     * 32736 pages / 1024
     * -> 31 remainder
     * -> 32 tables required
     */
    table_count =
        (
            total_pages +
            PAGE_TABLE_ENTRIES -
            1U
        ) /
        PAGE_TABLE_ENTRIES;


    /*
     * Do not exceed the statically reserved
     * page table area.
     */
    if (table_count >
        PAGING_MAX_PAGE_TABLES)
    {
        return 0;
    }


    /*
     * Clear page directory.
     */
    for (
        uint32_t i = 0;
        i < PAGE_DIRECTORY_ENTRIES;
        ++i
    ) {
        page_directory[i] = 0;
    }


    /*
     * Clear all page tables.
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


    /*
     * Build identity mapping.
     *
     * Virtual address:
     *
     *     0x00123000
     *
     * maps to:
     *
     *     physical 0x00123000
     */
    for (
        uint32_t table = 0;
        table < table_count;
        ++table
    ) {

        /*
         * Physical address of this page table.
         *
         * Since paging is not enabled yet,
         * the current kernel address is directly
         * usable by the CPU.
         */
        uint32_t table_address =
            (uint32_t)(
                uintptr_t
            )&page_tables[table][0];


        /*
         * PDE:
         *
         * bit 0 = Present
         * bit 1 = Read/Write
         */
        page_directory[table] =
            table_address |
            0x003U;


        /*
         * Fill page table entries.
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


            /*
             * Do not map beyond actual PMM pages.
             */
            if (page >= total_pages)
                break;


            uint32_t physical_address =
                page *
                PAGE_SIZE;


            /*
             * PTE:
             *
             * bit 0 = Present
             * bit 1 = Read/Write
             */
            page_tables[table][entry] =
                physical_address |
                0x003U;


            mapped_pages++;
        }
    }


    return 1;
}


/*
 * ============================================================
 * Enable paging.
 *
 * Implemented using inline assembly.
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
     *
     * CR3 contains the physical address of
     * the page directory.
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
     * Set CR0.PG (bit 31).
     *
     * This enables paging.
     *
     * PE (bit 0) should already be enabled
     * by the existing protected-mode setup.
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


    paging_enabled = 1;
}


/*
 * ============================================================
 * Return paging state.
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
 * Return mapped page count.
 * ============================================================
 */
uint32_t paging_get_mapped_pages(void)
{
    return mapped_pages;
}