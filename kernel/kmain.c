#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "idt.h"

#define MULTIBOOT_MAGIC 0x2BADB002

void kmain(uint32_t magic, uint32_t mbi_addr);

static void test_exception(void) {
    /*
     * UD2 deliberately generates Invalid Opcode (#UD), interrupt 6.
     */
    __asm__ volatile ("ud2");
}

void kmain(uint32_t magic, uint32_t mbi_addr) {
    (void)mbi_addr;

    vga_init();

    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    vga_write("firstOS v0.0.3\n");

    vga_setcolor(VGA_LGREY, VGA_BLACK);
    vga_write("----------------------------------------\n");

    if (magic != MULTIBOOT_MAGIC) {
        vga_setcolor(VGA_LRED, VGA_BLACK);
        vga_write("FATAL: bad multiboot magic\n");

        for (;;) {
            __asm__ volatile ("cli");
            __asm__ volatile ("hlt");
        }
    }

    vga_setcolor(VGA_LGREEN, VGA_BLACK);
    vga_write("[ ok ] multiboot verified\n");
    vga_write("[ ok ] vga text mode ready\n");

    gdt_init();

    vga_write("[ ok ] gdt loaded\n");
    vga_write("[ ok ] kernel code segment ready\n");
    vga_write("[ ok ] kernel data segment ready\n");

    idt_init();

    vga_write("[ ok ] idt loaded\n");
    vga_write("[ ok ] exception handlers installed\n");

    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    vga_write("[test] generating invalid opcode (#UD)...\n");

    test_exception();

    vga_setcolor(VGA_LRED, VGA_BLACK);
    vga_write("[fail] exception did not fire\n");

    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}
