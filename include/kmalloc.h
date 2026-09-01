#ifndef FIRSTOS_KMALLOC_H
#define FIRSTOS_KMALLOC_H

#include <stdint.h>

/*
 * Initialize kernel heap.
 *
 * The current implementation is lazy:
 * no physical page is allocated here.
 * Pages are requested from PMM when kmalloc()
 * actually needs them.
 */
void kmalloc_init(void);


/*
 * Allocate a block of kernel memory.
 *
 * Returns:
 *
 *   pointer to usable memory
 *
 * Returns 0 if allocation fails.
 */
void *kmalloc(uint32_t size);


/*
 * Free a previously allocated block.
 */
void kfree(void *ptr);


/*
 * Return total number of heap pages acquired.
 */
uint32_t kmalloc_get_pages(void);


/*
 * Return number of bytes currently allocated.
 */
uint32_t kmalloc_get_used(void);


/*
 * Return number of bytes currently available
 * inside existing heap blocks.
 */
uint32_t kmalloc_get_free(void);

#endif