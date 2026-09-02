#include <stdint.h>

#include "syscall.h"
#include "serial.h"
#include "task.h"
#include "uaccess.h"

#define SYSCALL_WRITE_MAX 256U

struct registers *syscall_dispatch(
    struct registers *regs
)
{
    if (regs == 0)
        return regs;

    switch (regs->eax) {

        case SYS_WRITE:
        {
            uint32_t user_buffer =
                regs->ebx;

            uint32_t length =
                regs->ecx;

            uint8_t buffer[
                SYSCALL_WRITE_MAX
            ];

            /*
             * Prevent an oversized userspace request
             * from consuming excessive kernel stack.
             */
            if (
                length == 0U ||
                length > SYSCALL_WRITE_MAX
            ) {
                regs->eax =
                    (uint32_t)-1;

                return regs;
            }

            /*
             * Only a Ring 3 task may use the user
             * memory syscall interface.
             */
            if (
                (regs->cs & 0x3U) != 0x3U
            ) {
                regs->eax =
                    (uint32_t)-1;

                return regs;
            }

            if (
                !copy_from_user(
                    buffer,
                    user_buffer,
                    length
                )
            ) {
                serial_write(
                    "[syscall] SYS_WRITE invalid user range\n"
                );

                regs->eax =
                    (uint32_t)-1;

                return regs;
            }

            for (
                uint32_t i = 0;
                i < length;
                ++i
            ) {
                serial_putc(
                    (char)buffer[i]
                );
            }

            /*
             * Return number of bytes written.
             */
            regs->eax =
                length;

            return regs;
        }


        case SYS_YIELD:

            if (
                (regs->cs & 0x3U) == 0x3U
            ) {
                serial_write(
                    "[syscall] SYS_YIELD from Ring 3\n"
                );
            }

            return task_scheduler_tick(
                regs
            );


        default:

            regs->eax =
                (uint32_t)-1;

            return regs;
    }
}
