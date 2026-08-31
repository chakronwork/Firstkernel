#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 128

void keyboard_init(void);

void keyboard_handler(void);

int keyboard_readline(
    char *buffer,
    uint32_t size
);

#endif