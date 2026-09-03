.code32
.global _start
.text

_start:
    /*
     * Test 1: valid user buffer
     */
    mov $1, %eax
    mov $msg_before, %ebx
    mov $31, %ecx
    int $0x80

    /*
     * Test 2: sleep for 10 PIT ticks
     */
    mov $4, %eax
    mov $10, %ebx
    int $0x80

    /*
     * Execution should resume after wakeup.
     */
    mov $1, %eax
    mov $msg_after, %ebx
    mov $31, %ecx
    int $0x80

    /*
     * Exit cleanly.
     */
    mov $3, %eax
    int $0x80

1:
    jmp 1b

msg_before:
    .ascii "before SYS_SLEEP: PASS\n"

msg_after:
    .ascii "after SYS_SLEEP: PASS\n"
