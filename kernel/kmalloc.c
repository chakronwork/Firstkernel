#include <stdint.h>

#include "kmalloc.h"
#include "pmm.h"


/*
 * ============================================================
 * Configuration
 * ============================================================
 */

#define KMALLOC_ALIGNMENT 8U

#define KMALLOC_MAGIC 0x4B4D414CU


/*
 * Minimum split remainder:
 *
 * block header + minimum 8-byte payload
 */
#define KMALLOC_MIN_SPLIT \
    (sizeof(struct heap_block) + KMALLOC_ALIGNMENT)


/*
 * Maximum number of blocks that can safely
 * exist in one physical page is naturally
 * limited by the page size.
 */


/*
 * ============================================================
 * Heap block
 * ============================================================
 *
 * Memory:
 *
 *   +---------------------------+
 *   | struct heap_block         |
 *   +---------------------------+
 *   | user memory               |
 *   |                           |
 *   +---------------------------+
 */
struct heap_block {

    /*
     * Integrity marker.
     */
    uint32_t magic;

    /*
     * Payload size.
     */
    uint32_t size;

    /*
     * 1 = free
     * 0 = allocated
     */
    uint32_t free;

    /*
     * Doubly-linked list.
     */
    struct heap_block *next;
    struct heap_block *prev;
} __attribute__((aligned(KMALLOC_ALIGNMENT)));


/*
 * ============================================================
 * Heap state
 * ============================================================
 */

static struct heap_block *heap_head = 0;

static uint32_t heap_pages = 0;

static uint32_t heap_used = 0;


/*
 * ============================================================
 * Alignment
 * ============================================================
 */
static uint32_t align_up(uint32_t value)
{
    uint32_t remainder;

    remainder =
        value & (KMALLOC_ALIGNMENT - 1U);

    if (remainder == 0)
        return value;

    /*
     * Detect uint32 overflow.
     */
    if (value >
        0xFFFFFFFFU -
        (KMALLOC_ALIGNMENT - remainder))
    {
        return 0;
    }

    return
        value +
        (KMALLOC_ALIGNMENT - remainder);
}


/*
 * ============================================================
 * Block helpers
 * ============================================================
 */


/*
 * Check whether block magic is valid.
 */
static int block_magic_valid(
    const struct heap_block *block
)
{
    if (block == 0)
        return 0;

    return block->magic ==
           KMALLOC_MAGIC;
}


/*
 * Check whether two blocks are physically
 * adjacent.
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

    if (!block_magic_valid(a))
        return 0;

    if (!block_magic_valid(b))
        return 0;

    a_end =
        (uintptr_t)a +
        sizeof(struct heap_block) +
        (uintptr_t)a->size;

    if (a_end < (uintptr_t)a)
        return 0;

    expected =
        (uintptr_t)b;

    return a_end == expected;
}


/*
 * ============================================================
 * Heap validation
 * ============================================================
 *
 * This function walks the entire linked list and checks:
 *
 *   - magic
 *   - block size
 *   - alignment
 *   - prev/next relationships
 *   - address ordering
 *
 * It does NOT attempt to recover corrupted metadata.
 */
int kmalloc_validate(void)
{
    struct heap_block *block;
    struct heap_block *previous;

    uint32_t counted_used = 0;

    uint32_t guard = 0;


    block =
        heap_head;

    previous = 0;


    while (block != 0) {

        /*
         * Safety guard.
         *
         * A corrupted linked list could otherwise
         * produce an infinite loop.
         */
        guard++;

        if (guard > 65536U)
            return 0;


        /*
         * Validate block address alignment.
         */
        if (((uintptr_t)block &
             (KMALLOC_ALIGNMENT - 1U)) != 0)
        {
            return 0;
        }


        /*
         * Validate magic.
         */
        if (!block_magic_valid(block))
            return 0;


        /*
         * Payload size must be aligned.
         */
        if ((block->size &
             (KMALLOC_ALIGNMENT - 1U)) != 0)
        {
            return 0;
        }


        /*
         * A block cannot have zero payload.
         */
        if (block->size == 0)
            return 0;


        /*
         * prev pointer must agree.
         */
        if (block->prev != previous)
            return 0;


        /*
         * next pointer relationship.
         */
        if (block->next != 0) {

            if (block->next->prev != block)
                return 0;

            /*
             * Block list must be in ascending
             * virtual address order.
             */
            if ((uintptr_t)block->next <=
                (uintptr_t)block)
            {
                return 0;
            }
        }


        /*
         * Count allocated bytes.
         */
        if (!block->free) {

            /*
             * Protect counter from overflow.
             */
            if (counted_used >
                0xFFFFFFFFU -
                block->size)
            {
                return 0;
            }

            counted_used +=
                block->size;
        }


        previous =
            block;

        block =
            block->next;
    }


    /*
     * Internal accounting must match.
     */
    if (counted_used != heap_used)
        return 0;


    return 1;
}


/*
 * ============================================================
 * Split block
 * ============================================================
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

    if (!block_magic_valid(block))
        return;

    if (block->size <= requested_size)
        return;


    remaining =
        block->size -
        requested_size;


    if (remaining <
        KMALLOC_MIN_SPLIT)
    {
        return;
    }


    new_block =
        (struct heap_block *)(
            (uintptr_t)block +
            sizeof(struct heap_block) +
            requested_size
        );


    /*
     * Initialize new block.
     */
    new_block->magic =
        KMALLOC_MAGIC;

    new_block->size =
        remaining -
        sizeof(struct heap_block);

    new_block->free = 1;

    new_block->prev =
        block;

    new_block->next =
        block->next;


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
 * ============================================================
 * Merge block with next
 * ============================================================
 */
static void merge_with_next(
    struct heap_block *block
)
{
    struct heap_block *next;


    if (block == 0)
        return;

    if (!block_magic_valid(block))
        return;


    next =
        block->next;


    if (next == 0)
        return;

    if (!block_magic_valid(next))
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


    /*
     * Prevent arithmetic overflow.
     */
    if (block->size >
        0xFFFFFFFFU -
        sizeof(struct heap_block) -
        next->size)
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


    /*
     * Clear merged-out block metadata.
     *
     * This makes accidental reuse easier to detect
     * during debugging.
     */
    next->magic = 0;
    next->size = 0;
    next->free = 1;
    next->next = 0;
    next->prev = 0;
}


/*
 * ============================================================
 * Coalescing
 * ============================================================
 */
static void coalesce(
    struct heap_block *block
)
{
    if (block == 0)
        return;


    if (!block_magic_valid(block))
        return;


    /*
     * Merge forward.
     */
    merge_with_next(block);


    /*
     * Merge backward.
     */
    if (block->prev != 0) {

        struct heap_block *previous =
            block->prev;


        if (block_magic_valid(previous) &&
            previous->free)
        {
            if (blocks_are_adjacent(
                    previous,
                    block
                ))
            {
                merge_with_next(
                    previous
                );
            }
        }
    }
}


/*
 * ============================================================
 * Grow heap
 * ============================================================
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
     * Before paging exists:
     *
     * physical address == current
     * kernel linear address.
     */
    block =
        (struct heap_block *)(
            (uintptr_t)physical_page
        );


    /*
     * Initialize block.
     */
    block->magic =
        KMALLOC_MAGIC;

    block->size =
        PMM_PAGE_SIZE -
        sizeof(struct heap_block);

    block->free = 1;

    block->next = 0;
    block->prev = 0;


    heap_pages++;


    /*
     * First page.
     */
    if (heap_head == 0) {

        heap_head =
            block;

        return block;
    }


    /*
     * Find last block.
     */
    last =
        heap_head;


    while (last->next != 0) {

        /*
         * Heap should already be valid.
         *
         * If corruption is detected, abandon
         * the operation rather than traversing
         * arbitrary memory indefinitely.
         */
        if (!block_magic_valid(last))
            return 0;

        last =
            last->next;
    }


    last->next =
        block;

    block->prev =
        last;


    return block;
}


/*
 * ============================================================
 * Initialize
 * ============================================================
 */
void kmalloc_init(void)
{
    heap_head = 0;

    heap_pages = 0;
    heap_used = 0;
}


/*
 * ============================================================
 * Allocate
 * ============================================================
 */
void *kmalloc(uint32_t size)
{
    struct heap_block *block;

    uint32_t aligned_size;


    /*
     * Zero-sized allocation is invalid.
     */
    if (size == 0)
        return 0;


    /*
     * Align requested size.
     */
    aligned_size =
        align_up(size);


    /*
     * Overflow.
     */
    if (aligned_size == 0)
        return 0;


    size =
        aligned_size;


    /*
     * Search existing free blocks.
     */
    block =
        heap_head;


    while (block != 0) {

        /*
         * Corrupted metadata:
         * do not continue.
         */
        if (!block_magic_valid(block))
            return 0;


        if (block->free &&
            block->size >= size)
        {
            split_block(
                block,
                size
            );


            block->free = 0;


            if (heap_used >
                0xFFFFFFFFU -
                block->size)
            {
                /*
                 * Undo allocation state.
                 */
                block->free = 1;

                return 0;
            }


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
     * No free block found.
     *
     * Grow heap.
     */
    block =
        grow_heap();


    if (block == 0)
        return 0;


    /*
     * A new physical page contains:
     *
     *   [block header][free payload]
     */
    split_block(
        block,
        size
    );


    /*
     * It might be impossible to satisfy
     * a request larger than one page.
     *
     * Current allocator intentionally does
     * not support multi-page contiguous
     * allocations yet.
     */
    if (block->size < size) {

        /*
         * Return the page to PMM only if this
         * is the only block in that page.
         *
         * For now, leave it as a free heap block
         * rather than attempting page rollback.
         */
        block->free = 1;

        return 0;
    }


    block->free = 0;


    if (heap_used >
        0xFFFFFFFFU -
        block->size)
    {
        block->free = 1;
        return 0;
    }


    heap_used +=
        block->size;


    return (void *)(
        (uintptr_t)block +
        sizeof(struct heap_block)
    );
}


/*
 * ============================================================
 * Free
 * ============================================================
 */
void kfree(void *ptr)
{
    struct heap_block *block;

    uintptr_t user_address;


    /*
     * NULL is always ignored.
     */
    if (ptr == 0)
        return;


    user_address =
        (uintptr_t)ptr;


    /*
     * DO NOT blindly subtract a header
     * from an arbitrary pointer.
     *
     * Search for the exact user pointer.
     */
    block =
        heap_head;


    while (block != 0) {

        /*
         * Corrupted heap.
         *
         * Refuse to continue.
         */
        if (!block_magic_valid(block))
            return;


        /*
         * Calculate exact user address.
         */
        uintptr_t expected =
            (uintptr_t)block +
            sizeof(struct heap_block);


        if (user_address == expected)
            break;


        block =
            block->next;
    }


    /*
     * Pointer was not allocated by this heap.
     */
    if (block == 0)
        return;


    /*
     * Double free.
     */
    if (block->free)
        return;


    /*
     * Accounting.
     */
    if (heap_used >= block->size)
        heap_used -= block->size;
    else
        heap_used = 0;


    block->free = 1;


    /*
     * Merge neighboring free blocks.
     */
    coalesce(block);
}


/*
 * ============================================================
 * Statistics
 * ============================================================
 */
uint32_t kmalloc_get_pages(void)
{
    return heap_pages;
}


uint32_t kmalloc_get_used(void)
{
    return heap_used;
}


uint32_t kmalloc_get_free(void)
{
    uint32_t total = 0;

    struct heap_block *block;


    block =
        heap_head;


    while (block != 0) {

        if (!block_magic_valid(block))
            return 0;


        if (block->free) {

            if (total >
                0xFFFFFFFFU -
                block->size)
            {
                return 0;
            }

            total +=
                block->size;
        }


        block =
            block->next;
    }


    return total;
}