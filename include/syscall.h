#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

#define SYS_WRITE     1U
#define SYS_YIELD     2U
#define SYS_EXIT      3U
#define SYS_SLEEP     4U
#define SYS_IPC_SEND  5U
#define SYS_IPC_RECV  6U

struct registers *syscall_dispatch(
    struct registers *regs
);

#endif /* SYSCALL_H */
