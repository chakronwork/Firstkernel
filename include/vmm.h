#ifndef FIRSTOS_VMM_H
#define FIRSTOS_VMM_H

#include <stdint.h>

#include "paging.h"


/*
 * ============================================================
 * Virtual memory range
 * ============================================================
 *
 * The bootstrap identity mapping currently occupies the
 * low virtual address range.
 *
 * VMM allocations therefore begin at 1 GiB.
 *
 * 0x40000000 -> 0xC0000000
 *
 * Size:
 *
 *     2 GiB
 *
 * Number of pages:
 *
 *     2 GiB / 4 KiB
 *     = 524288 pages
 *
 * The bitmap requires:
 *
 *     524288 bits
 *     = 65536 bytes
 */
#define VMM_START_ADDRESS 0x40000000U
#define VMM_END_ADDRESS   0xC0000000U

#define VMM_MAX_PAGES \
    ((VMM_END_ADDRESS - VMM_START_ADDRESS) / PAGE_SIZE)

#define VMM_BITMAP_WORDS \
    ((VMM_MAX_PAGES + 31U) / 32U)


/*
 * ============================================================
 * Initialization
 * ============================================================
 *
 * Initialize the virtual address allocator.
 *
 * Returns:
 *
 *     1 = success
 *     0 = failure
 */
int vmm_init(void);


/*
 * ============================================================
 * Allocate one page
 * ============================================================
 *
 * The function:
 *
 *     1. Finds a free virtual page.
 *     2. Allocates a physical frame from PMM.
 *     3. Maps the virtual page to that frame.
 *
 * Returns:
 *
 *     Virtual address
 *     0 on failure
 */
uint32_t vmm_alloc_page(
    uint32_t flags
);


/*
 * ============================================================
 * Allocate multiple contiguous pages
 * ============================================================
 *
 * Returns:
 *
 *     Starting virtual address
 *     0 on failure
 */
uint32_t vmm_alloc_pages(
    uint32_t count,
    uint32_t flags
);


/*
 * ============================================================
 * Free one page
 * ============================================================
 *
 * The physical frame backing the virtual page is returned
 * to PMM.
 *
 * Returns:
 *
 *     1 = success
 *     0 = failure
 */
int vmm_free_page(
    uint32_t virtual_address
);


/*
 * ============================================================
 * Free multiple contiguous pages
 * ============================================================
 *
 * Returns:
 *
 *     1 = success
 *     0 = failure
 */
int vmm_free_pages(
    uint32_t virtual_address,
    uint32_t count
);


/*
 * ============================================================
 * Query allocator usage
 * ============================================================
 */
uint32_t vmm_get_total_pages(void);

uint32_t vmm_get_used_pages(void);

uint32_t vmm_get_free_pages(void);


#endif