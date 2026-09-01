#ifndef FIRSTOS_PAGING_H
#define FIRSTOS_PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096U
#define PAGE_TABLE_ENTRIES 1024U
#define PAGE_DIRECTORY_ENTRIES 1024U

/*
 * Current FirstOS bootstrap limit.
 *
 * 32 page tables × 4 MiB = 128 MiB.
 */
#define PAGING_MAX_PAGE_TABLES 32U

/*
 * Initialize identity-mapped paging.
 *
 * The number of mapped pages is taken from PMM.
 */
int paging_init(void);

/*
 * Enable paging using the prepared page directory.
 */
void paging_enable(void);

/*
 * Return whether paging is currently enabled.
 */
int paging_is_enabled(void);

/*
 * Return number of physical pages identity mapped.
 */
uint32_t paging_get_mapped_pages(void);

#endif