#ifndef FIRSTOS_PMM_H
#define FIRSTOS_PMM_H

#include <stdint.h>

/*
 * Physical memory page size.
 */
#define PMM_PAGE_SIZE 4096U

/*
 * i386 physical address space:
 *
 * 4 GiB / 4 KiB = 1,048,576 pages
 *
 * 1 bit per page:
 *
 * 1,048,576 / 8 = 131,072 bytes
 */
#define PMM_MAX_PAGES 1048576U
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8U)


/*
 * Initialize the Physical Memory Manager
 * using the Multiboot1 memory map.
 */
void pmm_init(uint32_t multiboot_info_addr);


/*
 * Allocate one physical 4 KiB page.
 *
 * Returns:
 *
 *   physical address
 *
 * Returns 0 if no page is available.
 */
uint32_t pmm_alloc_page(void);


/*
 * Free one physical 4 KiB page.
 */
void pmm_free_page(uint32_t physical_address);


/*
 * Return total number of physical pages
 * known to the PMM.
 */
uint32_t pmm_get_total_pages(void);


/*
 * Return number of currently free pages.
 */
uint32_t pmm_get_free_pages(void);

#endif