#ifndef FIRSTOS_PAGE_FAULT_H
#define FIRSTOS_PAGE_FAULT_H

#include <stdint.h>

#include "idt.h"


/*
 * Handle CPU Page Fault exception.
 *
 * Exception vector:
 *
 *   14
 *
 * The CPU provides an error code for #PF.
 */
void page_fault_handler(
    struct registers *regs
);


/*
 * Read CR2.
 *
 * CR2 contains the virtual address
 * that caused the page fault.
 */
uint32_t page_fault_get_cr2(void);

#endif