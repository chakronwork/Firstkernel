#include "vga.h"

#define VGA_W 80
#define VGA_H 25

static uint16_t *const buf = (uint16_t *)0xB8000;
static size_t row, col;
static uint8_t color;

static inline uint16_t entry(char c, uint8_t co) {
    return (uint16_t)(unsigned char)c | ((uint16_t)co << 8);
}

void vga_setcolor(enum vga_color fg, enum vga_color bg) {
    color = (uint8_t)fg | ((uint8_t)bg << 4);
}

void vga_init(void) {
    row = col = 0;
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    for (size_t y = 0; y < VGA_H; y++)
        for (size_t x = 0; x < VGA_W; x++)
            buf[y * VGA_W + x] = entry(' ', color);
}

static void scroll(void) {
    for (size_t y = 1; y < VGA_H; y++)
        for (size_t x = 0; x < VGA_W; x++)
            buf[(y - 1) * VGA_W + x] = buf[y * VGA_W + x];
    for (size_t x = 0; x < VGA_W; x++)
        buf[(VGA_H - 1) * VGA_W + x] = entry(' ', color);
    row = VGA_H - 1;
}

void vga_putc(char c) {
    if (c == '\n') { col = 0; if (++row == VGA_H) scroll(); return; }
    buf[row * VGA_W + col] = entry(c, color);
    if (++col == VGA_W) { col = 0; if (++row == VGA_H) scroll(); }
}

void vga_write(const char *s) {
    while (*s) vga_putc(*s++);
}
