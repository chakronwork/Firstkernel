#ifndef FIRSTOS_KMALLOC_H
#define FIRSTOS_KMALLOC_H

#include <stdint.h>


/*
 * Initialize kernel heap.
 */
void kmalloc_init(void);


/*
 * Allocate kernel memory.
 *
 * Returns:
 *
 *   pointer to usable memory
 *
 * Returns 0 if allocation fails.
 */
void *kmalloc(uint32_t size);


/*
 * Free previously allocated memory.
 *
 * Invalid pointers and double frees are ignored.
 */
void kfree(void *ptr);


/*
 * Validate the entire heap structure.
 *
 * Returns:
 *
 *   1 = valid
 *   0 = corruption detected
 */
int kmalloc_validate(void);


/*
 * Return number of heap pages acquired.
 */
uint32_t kmalloc_get_pages(void);


/*
 * Return bytes currently allocated.
 */
uint32_t kmalloc_get_used(void);


/*
 * Return bytes available inside free blocks.
 */
uint32_t kmalloc_get_free(void);

#endif