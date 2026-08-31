#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include "vga.h"

void console_init(void);

void console_putc(char c);
void console_write(const char *str);

void console_backspace(void);

void console_clear(void);

void console_set_color(
    uint8_t foreground,
    uint8_t background
);

#endif