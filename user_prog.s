.code32
.global _start
.text
_start:
    mov $1, %eax
    mov $msg, %ebx
    mov $33, %ecx
    int $0x80

    mov $3, %eax
    int $0x80

1:
    jmp 1b

msg:
    .ascii "Hello from Initrd binary module!\n"
