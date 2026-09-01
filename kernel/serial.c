#include <stdint.h>

#include "serial.h"

#define COM1 0x3F8

#define SERIAL_DATA       (COM1 + 0)
#define SERIAL_INTERRUPT  (COM1 + 1)
#define SERIAL_FIFO       (COM1 + 2)
#define SERIAL_LINE       (COM1 + 3)
#define SERIAL_MODEM      (COM1 + 4)
#define SERIAL_STATUS     (COM1 + 5)

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

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

static int serial_transmit_empty(void)
{
    return (inb(SERIAL_STATUS) & 0x20) != 0;
}

void serial_init(void)
{
    /*
     * Disable interrupts.
     */
    outb(SERIAL_INTERRUPT, 0x00);

    /*
     * Enable DLAB.
     */
    outb(SERIAL_LINE, 0x80);

    /*
     * Baud rate divisor = 3
     *
     * 115200 / 3 = 38400 baud
     */
    outb(SERIAL_DATA, 0x03);
    outb(SERIAL_INTERRUPT, 0x00);

    /*
     * 8 bits, no parity, one stop bit.
     */
    outb(SERIAL_LINE, 0x03);

    /*
     * Enable FIFO.
     */
    outb(SERIAL_FIFO, 0xC7);

    /*
     * IRQs disabled, RTS/DSR enabled.
     */
    outb(SERIAL_MODEM, 0x03);
}

void serial_putc(char c)
{
    while (!serial_transmit_empty()) {
    }

    outb(SERIAL_DATA, (uint8_t)c);
}

void serial_write(const char *str)
{
    if (str == 0)
        return;

    while (*str != '\0') {
        if (*str == '\n')
            serial_putc('\r');

        serial_putc(*str);
        str++;
    }
}

void serial_write_hex32(uint32_t value)
{
    static const char hex[] =
        "0123456789ABCDEF";

    serial_write("0x");

    for (int i = 7; i >= 0; --i) {
        serial_putc(
            hex[(value >> (i * 4)) & 0xF]
        );
    }
}

void serial_write_dec(uint32_t value)
{
    char buffer[11];
    uint32_t i = 0;

    if (value == 0) {
        serial_putc('0');
        return;
    }

    while (value > 0) {
        buffer[i++] =
            (char)('0' + (value % 10));

        value /= 10;
    }

    while (i > 0) {
        serial_putc(buffer[--i]);
    }
}