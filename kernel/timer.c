#include <stdint.h>

#include "timer.h"
#include "pic.h"

static volatile uint32_t ticks = 0;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

void timer_handler(void)
{
    /*
     * IRQ0 fires once for every PIT tick.
     */
    ticks++;

    /*
     * Notify the master PIC that IRQ0 has been handled.
     */
    pic_send_eoi(0);
}

void timer_init(uint32_t frequency)
{
    uint32_t divisor;

    /*
     * Prevent division by zero.
     */
    if (frequency == 0)
        frequency = 100;

    /*
     * PIT base frequency:
     *
     * 1,193,180 Hz
     *
     * divisor = base frequency / requested frequency
     */
    divisor = 1193180 / frequency;

    /*
     * PIT command register
     *
     * 0x36:
     *
     * Channel 0
     * Access mode: low byte then high byte
     * Mode 3: square wave generator
     * Binary mode
     */
    outb(0x43, 0x36);

    /*
     * Send divisor to channel 0.
     */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void)
{
    return ticks;
}