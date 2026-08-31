.section .text

.global gdt_flush
.type gdt_flush, @function

gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ljmp $0x08, $reload_cs

reload_cs:
    ret

.size gdt_flush, . - gdt_flush
.section .note.GNU-stack,"",@progbits
