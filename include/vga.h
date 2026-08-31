#ifndef FIRSTOS_VGA_H
#define FIRSTOS_VGA_H

#include <stddef.h>
#include <stdint.h>

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LGREY = 7,

    VGA_DGREY = 8,
    VGA_LBLUE = 9,
    VGA_LGREEN = 10,
    VGA_LCYAN = 11,
    VGA_LRED = 12,
    VGA_LMAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
};

void vga_init(void);

void vga_setcolor(
    enum vga_color foreground,
    enum vga_color background
);

void vga_putc(char c);

void vga_write(const char *s);

void vga_backspace(void);

#endif