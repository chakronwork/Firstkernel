#ifndef FIRSTOS_PAGING_H
#define FIRSTOS_PAGING_H

#include <stdint.h>


/*
 * Page size.
 */
#define PAGE_SIZE 4096U


/*
 * Entries per page table.
 */
#define PAGE_TABLE_ENTRIES 1024U


/*
 * Entries per page directory.
 */
#define PAGE_DIRECTORY_ENTRIES 1024U


/*
 * Bootstrap page-table count.
 *
 * 32 tables × 4 MiB = 128 MiB
 */
#define PAGING_MAX_PAGE_TABLES 32U


/*
 * Page flags.
 */
#define PAGE_PRESENT 0x001U
#define PAGE_WRITABLE 0x002U
#define PAGE_USER 0x004U


/*
 * Initialize bootstrap identity mapping.
 */
int paging_init(void);


/*
 * Enable paging.
 */
void paging_enable(void);


/*
 * Return whether paging is enabled.
 */
int paging_is_enabled(void);


/*
 * Return number of pages currently mapped
 * by the bootstrap identity mapping.
 */
uint32_t paging_get_mapped_pages(void);


/*
 * Map one virtual page to one physical page.
 *
 * virtual_address:
 *     page-aligned virtual address
 *
 * physical_address:
 *     page-aligned physical address
 *
 * flags:
 *     PAGE_PRESENT
 *     PAGE_WRITABLE
 *     PAGE_USER
 *
 * Returns:
 *
 *     1 = success
 *     0 = failure
 */
int paging_map_page(
    uint32_t virtual_address,
    uint32_t physical_address,
    uint32_t flags
);


/*
 * Unmap one virtual page.
 *
 * Returns:
 *
 *     1 = page was mapped and is now unmapped
 *     0 = page was not mapped
 */
int paging_unmap_page(
    uint32_t virtual_address
);


/*
 * Query one virtual page.
 *
 * physical_address:
 *     receives mapped physical page
 *
 * flags:
 *     receives current PTE flags
 *
 * Returns:
 *
 *     1 = mapped
 *     0 = not mapped
 */
int paging_get_page(
    uint32_t virtual_address,
    uint32_t *physical_address,
    uint32_t *flags
);

#endif