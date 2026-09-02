#include <stdint.h>

#include "syscall.h"
#include "serial.h"
#include "task.h"

struct registers *syscall_dispatch(
    struct registers *regs
)
{
    if (regs == 0)
        return regs;

    switch (regs->eax) {

        case SYS_WRITE:

            serial_write(
                "[syscall] SYS_WRITE from Ring 3\n"
            );

            /*
             * Return value:
             *
             * EAX = 0
             *
             * Real user-buffer handling will be added
             * after user-pointer validation exists.
             */
            regs->eax = 0;

            return regs;


        case SYS_YIELD:

            /*
             * Reuse the existing scheduler path.
             *
             * This may return another task's saved
             * interrupt frame.
             */
            return task_scheduler_tick(
                regs
            );


        default:

            /*
             * Unknown syscall.
             */
            regs->eax = (uint32_t)-1;

            return regs;
    }
}
