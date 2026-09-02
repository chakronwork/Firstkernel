#include "gdt.h"
#include "tss.h"
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;

static struct tss_entry tss;

static uint8_t kernel_stack[4096]
    __attribute__((aligned(16)));

extern void gdt_flush(uint32_t);

static void gdt_set_gate(
    int num,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity
)
{
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = limit & 0xFFFF;

    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= granularity & 0xF0;

    gdt[num].access = access;
}

void tss_init(void)
{
    uint32_t stack_top =
        (uint32_t)(kernel_stack + sizeof(kernel_stack));

    for (uint32_t i = 0; i < sizeof(tss) / sizeof(uint32_t); ++i) {
        ((uint32_t *)&tss)[i] = 0;
    }

    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = stack_top;

    /*
     * I/O bitmap starts immediately after the TSS.
     * This effectively denies access to I/O ports from this TSS.
     */
    tss.iomap_base = sizeof(struct tss_entry);

    /*
     * 32-bit available TSS:
     * present = 1
     * DPL     = 0
     * type    = 0x9
     *
     * access = 0x89
     */
    gdt_set_gate(
        5,
        (uint32_t)&tss,
        sizeof(struct tss_entry) - 1,
        0x89,
        0x00
    );

    uint16_t tss_selector = GDT_TSS;

    __asm__ volatile (
        "ltr %0"
        :
        : "r"(tss_selector)
    );
}

void gdt_init(void)
{
    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint32_t)&gdt;

    /* NULL descriptor */
    gdt_set_gate(
        0,
        0,
        0,
        0,
        0
    );

    /* Kernel code */
    gdt_set_gate(
        1,
        0,
        0xFFFFFFFF,
        0x9A,
        0xCF
    );

    /* Kernel data */
    gdt_set_gate(
        2,
        0,
        0xFFFFFFFF,
        0x92,
        0xCF
    );

    /* User code */
    gdt_set_gate(
        3,
        0,
        0xFFFFFFFF,
        0xFA,
        0xCF
    );

    /* User data */
    gdt_set_gate(
        4,
        0,
        0xFFFFFFFF,
        0xF2,
        0xCF
    );

    /*
     * Load GDT first.
     * TSS selector points at GDT entry #5,
     * so LTR must happen after LGDT.
     */
    gdt_flush((uint32_t)&gp);

    tss_init();
}
