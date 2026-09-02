#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

#define SYS_WRITE  1U
#define SYS_YIELD  2U

struct registers *syscall_dispatch(
    struct registers *regs
);

#endif /* SYSCALL_H */
