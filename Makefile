CC = gcc
CFLAGS = -std=gnu11 -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude -fno-stack-protector -fno-pie -mno-red-zone
LDFLAGS = -m32 -ffreestanding -O2 -nostdlib -no-pie -Wl,--build-id=none -Wl,-z,noexecstack -T linker.ld

OBJS = boot/boot.o \
       kernel/gdt.o kernel/gdt_asm.o \
       kernel/idt.o kernel/idt_asm.o \
       kernel/pic.o kernel/timer.o kernel/keyboard.o \
       kernel/console.o kernel/serial.o kernel/vga.o \
       kernel/pmm.o kernel/kmalloc.o kernel/paging.o kernel/page_fault.o kernel/vmm.o \
       kernel/address_space.o kernel/task.o kernel/task_asm.o \
       kernel/syscall.o kernel/ipc.o kernel/uaccess.o kernel/initrd.o \
       kernel/user_test.o kernel/user_mode.o kernel/kmain.o

BIN = firstos.bin
ISO = firstos.iso

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(BIN) -lgcc

boot/boot.o: boot/boot.s
	$(CC) $(CFLAGS) -c boot/boot.s -o boot/boot.o

kernel/gdt_asm.o: kernel/gdt.s
	$(CC) $(CFLAGS) -c kernel/gdt.s -o kernel/gdt_asm.o

kernel/idt_asm.o: kernel/idt.s
	$(CC) $(CFLAGS) -c kernel/idt.s -o kernel/idt_asm.o

kernel/task_asm.o: kernel/task_asm.s
	$(CC) $(CFLAGS) -c kernel/task_asm.s -o kernel/task_asm.o

kernel/user_mode.o: kernel/user_mode.s
	$(CC) $(CFLAGS) -c kernel/user_mode.s -o kernel/user_mode.o

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

initrd.img: user_prog.s
	$(CC) -m32 -ffreestanding -nostdlib -no-pie -e _start -Wl,-Ttext=0x40000000 user_prog.s -o user_prog.elf
	objcopy -O binary --only-section=.text user_prog.elf initrd.img

iso: $(BIN) initrd.img
	mkdir -p iso/boot/grub
	cp $(BIN) iso/boot/
	cp initrd.img iso/boot/
	echo "set default=0" > iso/boot/grub/grub.cfg
	echo "set timeout=0" >> iso/boot/grub/grub.cfg
	echo 'menuentry "firstOS" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot /boot/$(BIN)' >> iso/boot/grub/grub.cfg
	echo '    module /boot/initrd.img' >> iso/boot/grub/grub.cfg
	echo '    boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

clean:
	rm -rf $(OBJS) $(BIN) $(ISO) user_prog.elf initrd.img iso

.PHONY: all iso clean
