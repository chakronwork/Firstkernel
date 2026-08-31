CC      := i686-elf-gcc
AS      := i686-elf-as
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude \
           -fno-stack-protector -fno-pic -mno-red-zone
LDFLAGS := -ffreestanding -O2 -nostdlib -T linker.ld

OBJS := boot/boot.o kernel/kmain.o kernel/vga.o

all: firstos.bin

%.o: %.s
	$(AS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

firstos.bin: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -lgcc

run: firstos.bin
	qemu-system-i386 -kernel firstos.bin -m 128M

iso: firstos.bin
	mkdir -p iso/boot/grub
	cp firstos.bin iso/boot/
	printf 'menuentry "firstOS" {\\n  multiboot /boot/firstos.bin\\n}\\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o firstos.iso iso

clean:
	rm -rf $(OBJS) firstos.bin firstos.iso iso

.PHONY: all run iso clean
