CC := gcc
AS := gcc

CFLAGS := -std=gnu11 -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude \
          -fno-stack-protector \
          -fno-pie \
          -mno-red-zone

LDFLAGS := -m32 -ffreestanding -O2 -nostdlib -no-pie \
           -Wl,--build-id=none \
           -Wl,-z,noexecstack \
           -T linker.ld


OBJS := \
    boot/boot.o \
    kernel/gdt.o \
    kernel/gdt_asm.o \
    kernel/idt.o \
    kernel/idt_asm.o \
    kernel/pic.o \
    kernel/timer.o \
    kernel/keyboard.o \
    kernel/console.o \
    kernel/serial.o \
    kernel/pmm.o \
    kernel/kmalloc.o \
    kernel/kmain.o \
    kernel/vga.o


all: firstos.bin


boot/boot.o: boot/boot.s
	$(AS) $(CFLAGS) -c $< -o $@


kernel/gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/gdt_asm.o: kernel/gdt.s
	$(AS) $(CFLAGS) -c $< -o $@


kernel/idt.o: kernel/idt.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/idt_asm.o: kernel/idt.s
	$(AS) $(CFLAGS) -c $< -o $@


kernel/pic.o: kernel/pic.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/keyboard.o: kernel/keyboard.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/console.o: kernel/console.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/serial.o: kernel/serial.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/pmm.o: kernel/pmm.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/kmalloc.o: kernel/kmalloc.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/kmain.o: kernel/kmain.c
	$(CC) $(CFLAGS) -c $< -o $@


kernel/vga.o: kernel/vga.c
	$(CC) $(CFLAGS) -c $< -o $@


firstos.bin: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -lgcc


run: firstos.bin
	qemu-system-i386 -kernel firstos.bin -m 128M -serial stdio


iso: firstos.bin
	mkdir -p iso/boot/grub
	cp firstos.bin iso/boot/

	printf 'menuentry "firstOS" {\\n  multiboot /boot/firstos.bin\\n}\\n' \
		> iso/boot/grub/grub.cfg

	grub-mkrescue -o firstos.iso iso


clean:
	rm -rf $(OBJS) firstos.bin firstos.iso iso


.PHONY: all run iso clean