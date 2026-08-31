#include "console.h"
#include "vga.h"

void console_init(void)
{
    vga_init();
}

void console_putc(char c)
{
    vga_putc(c);
}

void console_write(const char *str)
{
    vga_write(str);
}

void console_backspace(void)
{
    vga_backspace();
}

void console_clear(void)
{
    vga_init();
}

void console_set_color(
    uint8_t foreground,
    uint8_t background
)
{
    vga_setcolor(
        (enum vga_color)foreground,
        (enum vga_color)background
    );
}