#ifndef FIRSTOS_ADDRESS_SPACE_H
#define FIRSTOS_ADDRESS_SPACE_H

#include <stdint.h>

#include "paging.h"


/*
 * ============================================================
 * Address-space layout
 * ============================================================
 *
 * Lower 128 MiB:
 *
 *     kernel identity mapping
 *
 * This region is shared between address spaces.
 *
 *
 * User region:
 *
 *     0x40000000 -> 0xC0000000
 *
 * This region belongs to individual address spaces.
 */
#define ADDRESS_SPACE_KERNEL_END 0x08000000U

#define ADDRESS_SPACE_USER_START 0x40000000U
#define ADDRESS_SPACE_USER_END   0xC0000000U


/*
 * ============================================================
 * Page directory entries
 * ============================================================
 */
#define ADDRESS_SPACE_KERNEL_PDE_COUNT \
    (ADDRESS_SPACE_KERNEL_END >> 22)


/*
 * ============================================================
 * Address Space structure
 * ============================================================
 */
struct address_space {

    /*
     * Physical address of page directory.
     */
    uint32_t page_directory_physical;

    /*
     * Virtual pointer to page directory.
     *
     * Current firstOS paging model uses identity mapping,
     * so physical == virtual for bootstrap memory.
     */
    uint32_t *page_directory;

    /*
     * Number of page tables created by this address space.
     *
     * Kernel page tables are shared and therefore are not
     * counted here.
     */
    uint32_t owned_page_tables;

    /*
     * Number of user mappings.
     */
    uint32_t mapped_pages;

    /*
     * Whether this structure is valid.
     */
    int initialized;
};


/*
 * ============================================================
 * Create address space
 * ============================================================
 *
 * Returns:
 *
 *     pointer to address space
 *     0 on failure
 */
struct address_space *address_space_create(void);


/*
 * ============================================================
 * Switch active address space
 * ============================================================
 *
 * Returns:
 *
 *     1 = success
 *     0 = invalid address space
 */
int address_space_switch(
    struct address_space *space
);


/*
 * ============================================================
 * Query current address space
 * ============================================================
 */
uint32_t address_space_get_current_cr3(void);


/*
 * ============================================================
 * Map one user page
 * ============================================================
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
int address_space_map_page(
    struct address_space *space,
    uint32_t virtual_address,
    uint32_t physical_address,
    uint32_t flags
);


/*
 * ============================================================
 * Unmap one user page
 * ============================================================
 *
 * This removes the mapping only.
 *
 * It does NOT return the physical frame to PMM.
 *
 * Returns:
 *
 *     1 = success
 *     0 = failure
 */
int address_space_unmap_page(
    struct address_space *space,
    uint32_t virtual_address
);


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
);


/*
 * ============================================================
 * Destroy address space
 * ============================================================
 *
 * Frees:
 *
 *     - page directory
 *     - page tables owned by address space
 *
 * It does NOT free physical frames referenced by user PTEs.
 */
int address_space_destroy(
    struct address_space *space
);


/*
 * ============================================================
 * Statistics
 * ============================================================
 */
uint32_t address_space_get_mapped_pages(
    const struct address_space *space
);

uint32_t address_space_get_owned_page_tables(
    const struct address_space *space
);


#endif