.section .text


.global idt_flush
.type idt_flush, @function

idt_flush:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

.size idt_flush, . - idt_flush


/*
 * Common ISR entry point.
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
 */
.global isr_common
.type isr_common, @function

isr_common:
    pusha

    /*
     * Clear direction flag.
     */
    cld

    /*
     * Pass pointer to registers structure.
     */
    push %esp
    call isr_handler
    add $4, %esp

    /*
     * Restore general-purpose registers.
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
 * ISR without CPU-provided error code.
 *
 * We push:
 *
 *   fake error code = 0
 *   interrupt number
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
 * ISR with CPU-provided error code.
 *
 * CPU already pushed:
 *
 *   error code
 *
 * We only push:
 *
 *   interrupt number
 */
.macro ISR_ERR n

.global isr\n
.type isr\n, @function

isr\n:
    push $\n
    jmp isr_common

.endm


/*
 * CPU Exceptions 0-31
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
 * Hardware IRQs
 *
 * IRQ0 -> vector 32
 * IRQ1 -> vector 33
 */

ISR_NOERR 32
ISR_NOERR 33


.section .note.GNU-stack,"",@progbits