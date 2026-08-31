#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct registers {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t int_no;
    uint32_t err_code;
};

void idt_init(void);
void isr_handler(struct registers *regs);

#endif