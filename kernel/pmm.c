#include <stdint.h>

#include "pmm.h"
#include "console.h"


/*
 * Multiboot1 flags.
 *
 * Bit 6:
 *   memory map is available.
 */
#define MULTIBOOT_INFO_MEMORY_MAP (1U << 6)


/*
 * Multiboot memory map entry type.
 *
 * 1 = available RAM.
 */
#define MULTIBOOT_MEMORY_AVAILABLE 1U


/*
 * Minimal Multiboot1 information structure.
 *
 * We only need:
 *
 *   flags
 *   mmap_length
 *   mmap_addr
 */
struct multiboot_info {
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;
    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    uint32_t syms[4];

    uint32_t mmap_length;
    uint32_t mmap_addr;
};


/*
 * Multiboot1 module descriptor.
 *
 * GRUB stores each loaded module's physical range here.
 */
struct multiboot_mod_list {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
};


/*
 * Multiboot1 memory map entry.
 */
struct multiboot_mmap_entry {
    uint32_t size;

    uint32_t base_addr_low;
    uint32_t base_addr_high;

    uint32_t length_low;
    uint32_t length_high;

    uint32_t type;
};


/*
 * Linker symbols.
 *
 * linker.ld defines these around the kernel image.
 */
extern uint8_t __kernel_start;
extern uint8_t __kernel_end;


/*
 * Physical page bitmap.
 *
 * 0 = free
 * 1 = used
 *
 * 4 GiB / 4 KiB = 1,048,576 pages.
 */
static uint8_t bitmap[PMM_BITMAP_SIZE]
    __attribute__((aligned(PMM_PAGE_SIZE)));


/*
 * Highest physical page tracked by PMM.
 */
static uint32_t total_pages = 0;


/*
 * Number of currently free pages.
 */
static uint32_t free_pages = 0;


/*
 * Set bitmap bit.
 */
static inline void bitmap_set(uint32_t page)
{
    bitmap[page / 8U] |=
        (uint8_t)(1U << (page % 8U));
}


/*
 * Clear bitmap bit.
 */
static inline void bitmap_clear(uint32_t page)
{
    bitmap[page / 8U] &=
        (uint8_t)~(1U << (page % 8U));
}


/*
 * Test bitmap bit.
 */
static inline int bitmap_test(uint32_t page)
{
    return
        (bitmap[page / 8U] &
         (uint8_t)(1U << (page % 8U))) != 0;
}


/*
 * Mark one physical page as USED.
 */
static void mark_used(uint32_t physical_address)
{
    uint32_t page;

    /*
     * Physical address must be page aligned.
     */
    if ((physical_address &
         (PMM_PAGE_SIZE - 1U)) != 0)
    {
        return;
    }

    page =
        physical_address / PMM_PAGE_SIZE;

    if (page >= PMM_MAX_PAGES)
        return;

    /*
     * Only update counters if this page
     * was previously free.
     */
    if (!bitmap_test(page)) {
        bitmap_set(page);

        if (free_pages > 0)
            free_pages--;
    }
}


/*
 * Mark one physical page as FREE.
 */
static void mark_free(uint32_t physical_address)
{
    uint32_t page;

    /*
     * Physical address must be page aligned.
     */
    if ((physical_address &
         (PMM_PAGE_SIZE - 1U)) != 0)
    {
        return;
    }

    page =
        physical_address / PMM_PAGE_SIZE;

    if (page >= PMM_MAX_PAGES)
        return;

    /*
     * Only update counters if this page
     * was previously used.
     */
    if (bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}


/*
 * Mark an entire usable memory range as FREE.
 *
 * base:
 *   physical starting address
 *
 * length:
 *   range length in bytes
 */
static void mark_range_free(
    uint64_t base,
    uint64_t length
)
{
    uint64_t start;
    uint64_t end;

    const uint64_t pmm_limit =
        (uint64_t)PMM_MAX_PAGES *
        (uint64_t)PMM_PAGE_SIZE;


    /*
     * Empty region.
     */
    if (length == 0)
        return;


    /*
     * Align start upward.
     */
    start =
        (base + PMM_PAGE_SIZE - 1ULL) &
        ~(uint64_t)(PMM_PAGE_SIZE - 1U);


    /*
     * Calculate end.
     */
    end =
        base + length;


    /*
     * Align end downward.
     */
    end &=
        ~(uint64_t)(PMM_PAGE_SIZE - 1U);


    /*
     * Ignore regions entirely outside
     * the 32-bit physical address space.
     */
    if (start >= 0x100000000ULL)
        return;


    /*
     * Clamp to 4 GiB.
     */
    if (end > 0x100000000ULL)
        end = 0x100000000ULL;


    /*
     * Clamp to PMM bitmap capacity.
     */
    if (end > pmm_limit)
        end = pmm_limit;


    /*
     * Empty after alignment/clamping.
     */
    if (start >= end)
        return;


    /*
     * Update highest physical page.
     *
     * Example:
     *
     * end = 128 MiB
     *
     * total_pages =
     *   128 MiB / 4096
     */
    {
        uint64_t highest_page =
            end / PMM_PAGE_SIZE;

        if (highest_page > total_pages) {
            total_pages =
                (uint32_t)highest_page;
        }
    }


    /*
     * Mark every page in the usable range FREE.
     */
    for (
        uint64_t address = start;
        address < end;
        address += PMM_PAGE_SIZE
    ) {
        mark_free((uint32_t)address);
    }
}


/*
 * Reserve a physical range.
 *
 * All pages inside the range become USED.
 */
static void reserve_range(
    uint32_t start,
    uint32_t end
)
{
    uint32_t address;


    /*
     * Align start downward.
     */
    start &=
        ~(PMM_PAGE_SIZE - 1U);


    /*
     * Align end upward.
     */
    end =
        (end + PMM_PAGE_SIZE - 1U) &
        ~(PMM_PAGE_SIZE - 1U);


    for (
        address = start;
        address < end;
        address += PMM_PAGE_SIZE
    ) {
        mark_used(address);

        /*
         * Prevent 32-bit wraparound.
         */
        if (address > 0xFFFFF000U)
            break;
    }
}


/*
 * Initialize bitmap and parse Multiboot
 * memory map.
 */
static void bitmap_init(
    uint32_t multiboot_info_addr
)
{
    struct multiboot_info *mbi =
        (struct multiboot_info *)
        multiboot_info_addr;


    /*
     * Initially mark EVERYTHING as used.
     */
    for (
        uint32_t i = 0;
        i < PMM_BITMAP_SIZE;
        ++i
    ) {
        bitmap[i] = 0xFF;
    }


    total_pages = 0;
    free_pages = 0;


    /*
     * Verify that Multiboot supplied
     * a memory map.
     */
    if ((mbi->flags &
         MULTIBOOT_INFO_MEMORY_MAP) == 0)
    {
        console_set_color(
            VGA_LRED,
            VGA_BLACK
        );

        console_write(
            "[fatal] multiboot memory map unavailable\n"
        );

        for (;;) {
            __asm__ volatile ("cli");
            __asm__ volatile ("hlt");
        }
    }


    /*
     * Walk through the Multiboot memory map.
     */
    uint32_t current =
        mbi->mmap_addr;

    uint32_t end =
        mbi->mmap_addr +
        mbi->mmap_length;


    while (current < end) {

        struct multiboot_mmap_entry *entry =
            (struct multiboot_mmap_entry *)
            current;


        /*
         * Type 1 = available RAM.
         */
        if (entry->type ==
            MULTIBOOT_MEMORY_AVAILABLE)
        {
            uint64_t base =
                ((uint64_t)entry->base_addr_high << 32) |
                entry->base_addr_low;

            uint64_t length =
                ((uint64_t)entry->length_high << 32) |
                entry->length_low;


            mark_range_free(
                base,
                length
            );
        }


        /*
         * Multiboot's size field does not
         * include the size field itself.
         */
        if (entry->size == 0)
            break;

        current +=
            entry->size +
            sizeof(entry->size);
    }


    /*
     * Reserve the first 1 MiB.
     *
     * This protects:
     *
     *   BIOS / firmware
     *   VGA
     *   low memory structures
     *   boot-time data
     */
    reserve_range(
        0x00000000U,
        0x00100000U
    );


    /*
     * Reserve the kernel image.
     *
     * linker.ld defines:
     *
     *   __kernel_start
     *   __kernel_end
     */
    reserve_range(
        (uint32_t)&__kernel_start,
        (uint32_t)&__kernel_end
    );


    /*
     * Reserve all Multiboot modules.
     *
     * GRUB-loaded files such as initrd.img live in
     * physical memory outside the kernel image. They must
     * remain untouched while the kernel is running.
     */
    if (mbi->mods_count != 0 && mbi->mods_addr != 0) {
        struct multiboot_mod_list *mods =
            (struct multiboot_mod_list *)mbi->mods_addr;

        for (uint32_t i = 0; i < mbi->mods_count; ++i) {
            if (mods[i].mod_end > mods[i].mod_start) {
                reserve_range(
                    mods[i].mod_start,
                    mods[i].mod_end
                );
            }
        }
    }


    /*
     * Never allow PMM to exceed bitmap capacity.
     */
    if (total_pages > PMM_MAX_PAGES)
        total_pages = PMM_MAX_PAGES;
}


/*
 * Public PMM initialization.
 */
void pmm_init(uint32_t multiboot_info_addr)
{
    bitmap_init(multiboot_info_addr);
}


/*
 * Allocate one physical 4 KiB page.
 */
uint32_t pmm_alloc_page(void)
{
    for (
        uint32_t page = 0;
        page < total_pages;
        ++page
    ) {
        if (!bitmap_test(page)) {

            bitmap_set(page);

            if (free_pages > 0)
                free_pages--;

            return
                page * PMM_PAGE_SIZE;
        }
    }


    /*
     * Out of physical memory.
     */
    return 0;
}


/*
 * Free a physical page.
 */
void pmm_free_page(
    uint32_t physical_address
)
{
    uint32_t page;


    /*
     * Must be page aligned.
     */
    if ((physical_address &
         (PMM_PAGE_SIZE - 1U)) != 0)
    {
        return;
    }


    page =
        physical_address /
        PMM_PAGE_SIZE;


    /*
     * Ignore pages outside PMM range.
     */
    if (page >= total_pages)
        return;


    /*
     * Avoid double free.
     */
    if (!bitmap_test(page))
        return;


    bitmap_clear(page);
    free_pages++;
}


/*
 * Return total physical pages.
 */
uint32_t pmm_get_total_pages(void)
{
    return total_pages;
}


/*
 * Return free physical pages.
 */
uint32_t pmm_get_free_pages(void)
{
    return free_pages;
}