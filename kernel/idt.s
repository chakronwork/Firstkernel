.section .text

.global idt_flush
.type idt_flush, @function

idt_flush:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

.size idt_flush, . - idt_flush


.global isr_common
.type isr_common, @function

isr_common:
    pusha

    cld

    push %esp
    call isr_handler
    add $4, %esp

    popa

    add $8, %esp
    iret

.size isr_common, . - isr_common


.macro ISR_NOERR n
.global isr\n
.type isr\n, @function

isr\n:
    push $0
    push $\n
    jmp isr_common
.endm


.macro ISR_ERR n
.global isr\n
.type isr\n, @function

isr\n:
    push $\n
    jmp isr_common
.endm


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

.section .note.GNU-stack,"",@progbits
