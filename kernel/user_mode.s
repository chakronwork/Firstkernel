.section .text

/*
 * ============================================================
 * Enter Ring 3
 * ============================================================
 *
 * Arguments:
 *
 *     4(%esp)  = user EIP
 *     8(%esp)  = user ESP
 *
 * Selectors:
 *
 *     user CS = 0x1B
 *     user SS = 0x23
 *
 * iret frame required by x86:
 *
 *     SS
 *     ESP
 *     EFLAGS
 *     CS
 *     EIP
 */

.global ring3_enter
.type ring3_enter, @function

ring3_enter:

    /*
     * Save arguments BEFORE modifying ESP.
     */
    mov 4(%esp), %eax
    mov 8(%esp), %ebx

    /*
     * Disable hardware interrupts during
     * the privilege transition.
     */
    cli

    /*
     * Build iret frame.
     */

    /*
     * User SS
     */
    push $0x23

    /*
     * User ESP
     */
    push %ebx

    /*
     * User EFLAGS
     *
     * Bit 1 must be set.
     * IF is enabled.
     */
    push $0x202

    /*
     * User CS
     */
    push $0x1B

    /*
     * User EIP
     */
    push %eax

    /*
     * CPL0 -> CPL3
     */
    iret

.size ring3_enter, . - ring3_enter

.section .note.GNU-stack,"",@progbits
