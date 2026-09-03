.code32
.global _start
.text

_start:
    /*
     * Send a message from Ring 3 Task A to Task B.
     *
     * Current initial Ring 3 task IDs are:
     *
     *     Task A = 4
     *     Task B = 5
     *
     * SYS_IPC_SEND:
     *
     *     EAX = 5
     *     EBX = receiver task ID
     *     ECX = user buffer
     *     EDX = length
     */
    mov $5, %eax
    mov $5, %ebx
    mov $msg_ipc, %ecx
    mov $24, %edx
    int $0x80

    /*
     * Exit after successful send.
     */
    mov $3, %eax
    int $0x80

1:
    jmp 1b

msg_ipc:
    .ascii "IPC message from Task A\n"
