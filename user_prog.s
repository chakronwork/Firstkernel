.code32
.global _start
.text

_start:
    /*
     * --------------------------------------------------------
     * Test 1: valid user buffer
     * --------------------------------------------------------
     */
    mov $1, %eax
    mov $msg_valid, %ebx
    mov $35, %ecx
    int $0x80

    /*
     * --------------------------------------------------------
     * Test 2: completely unmapped user page
     *
     * Task A has:
     *
     *     0x40000000 -> code
     *     0x40001000 -> stack
     *
     * 0x40002000 is intentionally unmapped.
     * --------------------------------------------------------
     */
    mov $1, %eax
    mov $0x40002000, %ebx
    mov $1, %ecx
    int $0x80

    /*
     * If uaccess rejected the pointer,
     * syscall returns -1 in EAX.
     *
     * Continue with another negative test.
     */

    /*
     * --------------------------------------------------------
     * Test 3: range crosses from mapped page into
     * an unmapped page
     *
     * 0x40001000 -> mapped stack page
     * 0x40002000 -> unmapped
     *
     * Starting near the end of the stack page means the
     * requested range crosses the page boundary.
     * --------------------------------------------------------
     */
    mov $1, %eax
    mov $0x40001FF0, %ebx
    mov $32, %ecx
    int $0x80

    /*
     * --------------------------------------------------------
     * Test 4: valid buffer again
     *
     * Confirms that failed uaccess requests do not destroy
     * the task or corrupt the syscall path.
     * --------------------------------------------------------
     */
    mov $1, %eax
    mov $msg_after, %ebx
    mov $39, %ecx
    int $0x80

    /*
     * --------------------------------------------------------
     * Exit
     * --------------------------------------------------------
     */
    mov $3, %eax
    int $0x80

1:
    jmp 1b


msg_valid:
    .ascii "uaccess valid read: PASS\\n"

msg_after:
    .ascii "uaccess survived invalid accesses: PASS\\n"
