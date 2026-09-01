#include <stdint.h>

#include "kmalloc.h"
#include "pmm.h"


/*
 * Kernel heap block alignment.
 *
 * 8-byte alignment is enough for the current
 * 32-bit kernel and keeps returned addresses
 * suitably aligned for normal C objects.
 */
#define KMALLOC_ALIGNMENT 8U


/*
 * Minimum useful remainder when splitting
 * a free block.
 *
 * We need space for another block header
 * plus at least 8 bytes of payload.
 */
#define KMALLOC_MIN_SPLIT \
    (sizeof(struct heap_block) + KMALLOC_ALIGNMENT)


/*
 * Heap block header.
 *
 * Memory layout:
 *
 *   +----------------------+
 *   | struct heap_block    |
 *   +----------------------+
 *   | user memory          |
 *   |                      |
 *   +----------------------+
 *
 * The block itself lives inside a physical
 * page obtained from the PMM.
 */
struct heap_block {
    uint32_t size;

    uint32_t free;

    struct heap_block *next;
    struct heap_block *prev;
} __attribute__((aligned(KMALLOC_ALIGNMENT)));


/*
 * First block in the heap.
 */
static struct heap_block *heap_head = 0;


/*
 * Number of physical pages acquired by
 * the kernel heap.
 */
static uint32_t heap_pages = 0;


/*
 * Number of bytes currently allocated
 * to callers.
 */
static uint32_t heap_used = 0;


/*
 * Align value upward to 8 bytes.
 */
static uint32_t align_up(uint32_t value)
{
    return (
        value +
        KMALLOC_ALIGNMENT - 1U
    ) &
    ~(KMALLOC_ALIGNMENT - 1U);
}


/*
 * Check whether two blocks are physically
 * adjacent in memory.
 */
static int blocks_are_adjacent(
    struct heap_block *a,
    struct heap_block *b
)
{
    uintptr_t a_end;
    uintptr_t expected;

    if (a == 0 || b == 0)
        return 0;

    a_end =
        (uintptr_t)a +
        sizeof(struct heap_block) +
        a->size;

    expected =
        (uintptr_t)b;

    return a_end == expected;
}


/*
 * Split a block into:
 *
 *   [allocated block]
 *   [free block]
 *
 * Only split if the remainder is large enough
 * to hold another block header and usable data.
 */
static void split_block(
    struct heap_block *block,
    uint32_t requested_size
)
{
    uint32_t remaining;
    struct heap_block *new_block;

    if (block == 0)
        return;

    if (block->size <= requested_size)
        return;

    remaining =
        block->size - requested_size;

    if (remaining < KMALLOC_MIN_SPLIT)
        return;

    new_block =
        (struct heap_block *)(
            (uintptr_t)block +
            sizeof(struct heap_block) +
            requested_size
        );

    new_block->size =
        remaining -
        sizeof(struct heap_block);

    new_block->free = 1;

    new_block->prev = block;
    new_block->next = block->next;

    if (new_block->next != 0) {
        new_block->next->prev =
            new_block;
    }

    block->next =
        new_block;

    block->size =
        requested_size;
}


/*
 * Merge a block with its next free block.
 */
static void merge_with_next(
    struct heap_block *block
)
{
    struct heap_block *next;

    if (block == 0)
        return;

    next =
        block->next;

    if (next == 0)
        return;

    if (!next->free)
        return;

    if (!blocks_are_adjacent(
            block,
            next
        ))
    {
        return;
    }

    block->size +=
        sizeof(struct heap_block) +
        next->size;

    block->next =
        next->next;

    if (block->next != 0) {
        block->next->prev =
            block;
    }
}


/*
 * Coalesce neighboring free blocks.
 */
static void coalesce(
    struct heap_block *block
)
{
    if (block == 0)
        return;

    /*
     * Merge forward first.
     */
    merge_with_next(block);

    /*
     * Then merge backward.
     */
    if (block->prev != 0 &&
        block->prev->free)
    {
        if (blocks_are_adjacent(
                block->prev,
                block
            ))
        {
            merge_with_next(
                block->prev
            );
        }
    }
}


/*
 * Request one new physical page from PMM
 * and turn it into a free heap block.
 */
static struct heap_block *grow_heap(void)
{
    uint32_t physical_page;
    struct heap_block *block;
    struct heap_block *last;

    physical_page =
        pmm_alloc_page();

    if (physical_page == 0)
        return 0;

    /*
     * Before paging exists, physical address
     * and kernel linear address are equivalent
     * in the current environment.
     */
    block =
        (struct heap_block *)(uintptr_t)physical_page;

    block->size =
        PMM_PAGE_SIZE -
        sizeof(struct heap_block);

    block->free = 1;

    block->next = 0;
    block->prev = 0;

    heap_pages++;


    /*
     * First heap page.
     */
    if (heap_head == 0) {
        heap_head = block;
        return block;
    }


    /*
     * Find the end of the block list.
     */
    last = heap_head;

    while (last->next != 0) {
        last = last->next;
    }


    last->next =
        block;

    block->prev =
        last;

    return block;
}


/*
 * Initialize heap state.
 */
void kmalloc_init(void)
{
    heap_head = 0;

    heap_pages = 0;
    heap_used = 0;
}


/*
 * Allocate kernel memory.
 */
void *kmalloc(uint32_t size)
{
    struct heap_block *block;

    if (size == 0)
        return 0;

    size =
        align_up(size);


    /*
     * Search existing free blocks.
     */
    block =
        heap_head;

    while (block != 0) {

        if (block->free &&
            block->size >= size)
        {
            split_block(
                block,
                size
            );

            block->free = 0;

            heap_used +=
                block->size;

            return (void *)(
                (uintptr_t)block +
                sizeof(struct heap_block)
            );
        }

        block =
            block->next;
    }


    /*
     * No existing block can satisfy
     * the request.
     *
     * Acquire another physical page.
     */
    block =
        grow_heap();

    if (block == 0)
        return 0;


    /*
     * A newly acquired page is free.
     */
    split_block(
        block,
        size
    );

    block->free = 0;

    heap_used +=
        block->size;

    return (void *)(
        (uintptr_t)block +
        sizeof(struct heap_block)
    );
}


/*
 * Free kernel memory.
 */
void kfree(void *ptr)
{
    struct heap_block *block;

    if (ptr == 0)
        return;


    block =
        (struct heap_block *)(
            (uintptr_t)ptr -
            sizeof(struct heap_block)
        );


    /*
     * Protect against obvious double free.
     */
    if (block->free)
        return;


    block->free = 1;


    if (heap_used >= block->size)
        heap_used -= block->size;
    else
        heap_used = 0;


    coalesce(block);
}


/*
 * Return number of heap pages.
 */
uint32_t kmalloc_get_pages(void)
{
    return heap_pages;
}


/*
 * Return bytes currently allocated.
 */
uint32_t kmalloc_get_used(void)
{
    return heap_used;
}


/*
 * Return bytes available in free blocks.
 */
uint32_t kmalloc_get_free(void)
{
    uint32_t total = 0;
    struct heap_block *block;

    block =
        heap_head;

    while (block != 0) {

        if (block->free)
            total += block->size;

        block =
            block->next;
    }

    return total;
}