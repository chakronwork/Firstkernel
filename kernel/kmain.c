#include "vga.h"

#define MULTIBOOT_MAGIC 0x2BADB002

void kmain(uint32_t magic, uint32_t mbi_addr);

void kmain(uint32_t magic, uint32_t mbi_addr) {
    (void)mbi_addr;
    vga_init();

    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    vga_write("firstOS v0.0.1\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    vga_write("----------------------------------------\n");

    if (magic != MULTIBOOT_MAGIC) {
        vga_setcolor(VGA_LRED, VGA_BLACK);
        vga_write("FATAL: bad multiboot magic\n");
        for (;;) __asm__ volatile ("hlt");
    }

    vga_setcolor(VGA_LGREEN, VGA_BLACK);
    vga_write("[ ok ] multiboot verified\n");
    vga_write("[ ok ] vga text mode ready\n");
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    vga_write("[todo] gdt, idt, pmm, paging\n");

    for (;;) __asm__ volatile ("hlt");
}
