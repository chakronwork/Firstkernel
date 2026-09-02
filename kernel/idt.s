.section .text


/*
 * ============================================================
 * IDT flush
 * ============================================================
 */
.global idt_flush
.type idt_flush, @function

idt_flush:

    mov 4(%esp), %eax
    lidt (%eax)
    ret

.size idt_flush, . - idt_flush


/*
 * ============================================================
 * Common ISR entry point
 * ============================================================
 *
 * Before pusha:
 *
 *   esp + 0   = int_no
 *   esp + 4   = err_code
 *   esp + 8   = eip
 *   esp + 12  = cs
 *   esp + 16  = eflags
 *
 * After pusha:
 *
 *   esp + 0   = edi
 *   esp + 4   = esi
 *   esp + 8   = ebp
 *   esp + 12  = original esp
 *   esp + 16  = ebx
 *   esp + 20  = edx
 *   esp + 24  = ecx
 *   esp + 28  = eax
 *   esp + 32  = int_no
 *   esp + 36  = err_code
 *   esp + 40  = eip
 *   esp + 44  = cs
 *   esp + 48  = eflags
 */
.global isr_common
.type isr_common, @function

isr_common:

    /*
     * Save all general purpose registers.
     */
    pusha


    /*
     * Clear direction flag.
     */
    cld


    /*
     * Pass pointer to interrupt frame.
     */
    push %esp

    call isr_handler

    add $4, %esp


    /*
     * EAX contains the frame to restore.
     *
     * It may be:
     *
     *   current task
     *
     * or
     *
     *   another task
     */
    test %eax, %eax

    jz .restore_current


    /*
     * Switch to returned frame.
     */
    mov %eax, %esp


.restore_current:

    /*
     * Restore registers.
     */
    popa


    /*
     * Remove:
     *
     *   int_no
     *   err_code
     */
    add $8, %esp


    /*
     * Return from interrupt.
     */
    iret

.size isr_common, . - isr_common


/*
 * ============================================================
 * ISR without CPU error code
 * ============================================================
 */
.macro ISR_NOERR n

.global isr\n
.type isr\n, @function

isr\n:

    push $0
    push $\n

    jmp isr_common

.endm


/*
 * ============================================================
 * ISR with CPU error code
 * ============================================================
 */
.macro ISR_ERR n

.global isr\n
.type isr\n, @function

isr\n:

    push $\n

    jmp isr_common

.endm


/*
 * ============================================================
 * CPU exceptions
 * ============================================================
 */

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7

ISR_ERR   8

ISR_NOERR 9

ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14

ISR_NOERR 15
ISR_NOERR 16

ISR_ERR   17

ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28

ISR_ERR   29
ISR_ERR   30

ISR_NOERR 31


/*
 * ============================================================
 * Hardware IRQs
 * ============================================================
 */

ISR_NOERR 32
ISR_NOERR 33


/*
 * ============================================================
 * Software scheduler interrupt
 * ============================================================
 *
 * int 0x30
 *
 * Vector 48
 */
ISR_NOERR 48


.section .note.GNU-stack,"",@progbits