#include <stdint.h>

#include "keyboard.h"
#include "pic.h"
#include "console.h"

static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];

static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;

static volatile uint8_t line_ready = 0;

static uint8_t shift_pressed = 0;

static char line_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t line_length = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static uint32_t next_index(uint32_t index)
{
    return (index + 1) % KEYBOARD_BUFFER_SIZE;
}

static int buffer_is_empty(void)
{
    return buffer_head == buffer_tail;
}

static int buffer_is_full(void)
{
    return next_index(buffer_head) == buffer_tail;
}

static void buffer_push(char c)
{
    if (buffer_is_full())
        return;

    keyboard_buffer[buffer_head] = c;
    buffer_head = next_index(buffer_head);
}

static int buffer_pop(char *c)
{
    if (buffer_is_empty())
        return 0;

    *c = keyboard_buffer[buffer_tail];

    buffer_tail = next_index(buffer_tail);

    return 1;
}


/*
 * Basic Set 1 scancode -> ASCII.
 */
static const char scancode_ascii[128] = {
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',

    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',

    [0x39] = ' '
};


/*
 * Shifted characters.
 */
static const char scancode_shift_ascii[128] = {
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',

    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',

    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',

    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',

    [0x39] = ' '
};


void keyboard_init(void)
{
    buffer_head = 0;
    buffer_tail = 0;

    line_ready = 0;
    line_length = 0;

    shift_pressed = 0;
}


/*
 * IRQ1 keyboard handler.
 */
void keyboard_handler(void)
{
    uint8_t scancode = inb(0x60);

    /*
     * Left Shift pressed.
     */
    if (scancode == 0x2A) {
        shift_pressed = 1;
        pic_send_eoi(1);
        return;
    }

    /*
     * Right Shift pressed.
     */
    if (scancode == 0x36) {
        shift_pressed = 1;
        pic_send_eoi(1);
        return;
    }

    /*
     * Left Shift released.
     */
    if (scancode == 0xAA) {
        shift_pressed = 0;
        pic_send_eoi(1);
        return;
    }

    /*
     * Right Shift released.
     */
    if (scancode == 0xB6) {
        shift_pressed = 0;
        pic_send_eoi(1);
        return;
    }

    /*
     * Key release.
     */
    if (scancode & 0x80) {
        pic_send_eoi(1);
        return;
    }

    /*
     * Enter.
     */
    if (scancode == 0x1C) {

        if (line_length < KEYBOARD_BUFFER_SIZE - 1) {
            line_buffer[line_length] = '\n';
            line_length++;
        }

        buffer_push('\n');

        line_ready = 1;

        console_putc('\n');

        pic_send_eoi(1);
        return;
    }

    /*
     * Backspace.
     */
    if (scancode == 0x0E) {

        if (line_length > 0) {

            line_length--;

            line_buffer[line_length] = '\0';

            console_backspace();
        }

        pic_send_eoi(1);
        return;
    }

    /*
     * Normal key.
     */
    if (scancode < 128) {

        char c;

        if (shift_pressed)
            c = scancode_shift_ascii[scancode];
        else
            c = scancode_ascii[scancode];

        if (c != 0) {

            if (line_length < KEYBOARD_BUFFER_SIZE - 1) {

                line_buffer[line_length] = c;

                line_length++;

                line_buffer[line_length] = '\0';

                buffer_push(c);

                console_putc(c);
            }
        }
    }

    pic_send_eoi(1);
}


/*
 * Read one complete line.
 *
 * Returns:
 *
 *   > 0  number of characters
 *   0    invalid argument
 */
int keyboard_readline(char *buffer, uint32_t size)
{
    if (buffer == 0 || size == 0)
        return 0;

    /*
     * Wait until Enter has been pressed.
     */
    while (!line_ready) {
        __asm__ volatile ("hlt");
    }

    /*
     * Copy current line.
     */
    uint32_t count = line_length;

    if (count >= size)
        count = size - 1;

    for (uint32_t i = 0; i < count; ++i) {
        buffer[i] = line_buffer[i];
    }

    buffer[count] = '\0';

    /*
     * Reset current line.
     */
    line_length = 0;
    line_buffer[0] = '\0';

    line_ready = 0;

    /*
     * Drain keyboard event buffer.
     *
     * The current line has already been copied.
     */
    char c;

    while (buffer_pop(&c)) {
        if (c == '\n')
            break;
    }

    return (int)count;
}