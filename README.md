# firstOS

firstOS is a small x86 operating system kernel built as a learning project. It is not a full desktop OS or a polished production kernel. Instead, it is a hands-on experiment in low-level programming: booting a machine, entering protected mode, setting up basic infrastructure, and gradually building a tiny kernel that can print text, handle interrupts, and manage physical memory.

The project is intentionally simple and direct. It uses a custom linker script, raw assembly stubs, minimal C code, and QEMU for testing. If you are learning how operating systems actually start up, this project is a good place to see the real pieces come together.

## Why this project exists

This repository was created to explore how an OS kernel comes to life from the very first instruction executed by the CPU.

A lot of beginner projects stop at “hello world,” but an operating system starts much earlier:

- The machine boots in real mode
- A bootloader loads the kernel image
- The kernel is mapped into memory at a known address
- Protected mode is enabled
- Segments and interrupt tables are set up
- Device drivers are initialized
- The kernel enters a continuous loop and waits for work

firstOS is a compact version of that process. It does not try to be fancy. It focuses on the foundation and keeps the code readable enough for learning.

## What the kernel does right now

At the moment, the project includes a set of core kernel features that are common in early-stage OS development:

- Multiboot 1 boot entry
- 32-bit x86 kernel startup
- VGA text-mode console output
- Global Descriptor Table (GDT) initialization
- Interrupt Descriptor Table (IDT) setup
- PIC remapping for IRQ handling
- PIT timer setup
- Keyboard interrupt support
- Physical memory manager (PMM)
- Simple command loop in the kernel

This means the kernel is already capable of booting, printing status information, initializing hardware, allocating physical pages, and reading keyboard input in a very basic loop.

## Project structure

```text
firstos/
├── boot/
│   └── boot.s
├── include/
│   ├── console.h
│   ├── gdt.h
│   ├── idt.h
│   ├── keyboard.h
│   ├── pic.h
│   ├── pmm.h
│   ├── timer.h
│   └── vga.h
├── kernel/
│   ├── console.c
│   ├── gdt.c
│   ├── gdt.s
│   ├── idt.c
│   ├── idt.s
│   ├── keyboard.c
│   ├── kmain.c
│   ├── pic.c
│   ├── pmm.c
│   ├── timer.c
│   └── vga.c
├── boot.s
├── LICENSE
├── Makefile
├── README.md
├── linker.ld
└── ...
```

### Main parts

#### boot/
This folder contains the very first stage of the kernel boot process. The assembly file sets up the stack and jumps into the C kernel entry point.

#### include/
These are header files that define the kernel interfaces, device constants, and data structure declarations for VGA, GDT, IDT, PIC, timer, keyboard, and physical memory management.

#### kernel/
This is the heart of the OS. Here you will find the implementation of the console, interrupt handling, memory setup, timer logic, keyboard scanning, and the kernel main loop.

#### linker.ld
This file defines the kernel memory layout. It places the kernel at the correct virtual/physical address and organizes `.text`, `.data`, `.bss`, and other sections.

#### Makefile
The build script compiles the assembly and C sources into a 32-bit kernel binary and provides commands to run it in QEMU.

## Boot flow

The boot process is straightforward but important:

1. The system loads the kernel image using a Multiboot-compatible bootloader.
2. The entry point in `boot/boot.s` starts execution.
3. A stack is set up.
4. `kmain` is called with the Multiboot magic value and pointer information.
5. The kernel verifies the Multiboot header.
6. Console output is initialized so the system can print status messages.
7. The GDT is installed.
8. The IDT is configured.
9. PIC is remapped for IRQs.
10. The PIT timer is initialized.
11. Keyboard interrupts are enabled.
12. The physical memory manager starts tracking pages.
13. The kernel prints memory statistics and tests page allocation.
14. Interrupts are enabled.
15. The kernel enters a command loop and waits for input.

That is exactly the shape of a very early OS kernel: initialize hardware, build the skeleton, then keep running.

## Memory model and architecture

This project targets x86 in 32-bit protected mode. That is a classic educational choice because it makes the main concepts easier to see:

- segmentation via GDT
- interrupt handling via IDT
- memory page tracking via PMM
- direct hardware interaction via port I/O

The kernel is built with `-m32` and `-ffreestanding`, which means it does not depend on libc or a hosted runtime environment. It is designed to run in a bare-metal environment, not as a normal Linux or Windows user program.

## GDT and IDT

The Global Descriptor Table (GDT) is one of the first low-level structures a kernel sets up. It describes code and data segments, which is necessary before the CPU can correctly execute protected mode code.

The Interrupt Descriptor Table (IDT) enables the kernel to define handlers for exceptions and interrupts. These handlers are essential for:

- CPU exceptions
- timer interrupts
- keyboard interrupts
- more advanced device and system events later on

The project already wires in these structures and loads them during startup.

## VGA console

The VGA text-mode console is one of the most important early features in a kernel. It gives the system a way to output information before any higher-level UI or terminal exists.

The console implementation uses standard VGA memory layout and writes characters directly to the screen buffer. This makes debugging much easier because you can print status messages like:

- multiboot verification passed
- GDT loaded
- IDT loaded
- keyboard ready
- memory manager initialized

This is often the first sign that a bare-metal kernel is alive.

## Physical memory manager

The project includes a minimal physical memory manager. This is a critical part of any kernel because the OS needs a way to know:

- how much physical memory exists
- which pages are free
- which pages are already allocated
- how to allocate frames for future tasks or drivers

The `pmm_init` flow in the kernel reads the Multiboot information structure, initializes the memory map, and then starts tracking free pages.

The code also performs a simple allocation test:

- allocate a page
- allocate another page
- confirm success
- free both pages again

This is a very small but meaningful example of memory management in a kernel.

## Interrupt and timer handling

The project initializes the Programmable Interrupt Controller (PIC) and the Programmable Interval Timer (PIT):

- PIC remapping moves IRQs to a stable range in the IDT
- PIT sets a known timer frequency
- keyboard interrupts are enabled on IRQ1

This gives the kernel a basic mechanism for handling asynchronous events. As the project grows, more drivers can be added around this interrupt model.

## Keyboard input loop

Once the system is initialized, the kernel enters a simple loop that waits for keyboard input. This is a small but important milestone because it demonstrates that the kernel can now interact with hardware beyond just printing boot output.

The flow is intentionally basic:

- read characters from the keyboard buffer
- store them in a command array
- print the received text back to the screen
- display a shell prompt again

This is not a full shell yet, but it is the beginning of user interaction and command processing.

## Build and run

The project is built with `make`, using a GCC-based toolchain for 32-bit freestanding code.

### Requirements

You will typically need:

- GCC
- GNU ld
- QEMU (`qemu-system-i386`)
- GRUB tools if you want to build an ISO image
- A 32-bit cross-toolchain is optional depending on your local setup, but the project is written to compile as a freestanding 32-bit kernel

### Build kernel

```bash
make
```

This produces the kernel binary `firstos.bin`.

### Run in QEMU

```bash
make run
```

This launches the kernel in QEMU using the `-kernel` option.

### Build ISO image

```bash
make iso
```

This creates a bootable ISO using GRUB.

### Clean build artifacts

```bash
make clean
```

## Build conventions

The Makefile is intentionally simple:

- compile all `.c` files with `-m32 -ffreestanding`
- assemble `.s` files with GCC as assembler
- link them using a custom linker script
- produce a raw kernel binary

This keeps the project close to how bare-metal kernels are traditionally built without relying on a large build system.

## Why this project is useful

Even though this is a tiny kernel, it teaches several important concepts that appear in all serious OS projects:

- how the CPU starts in a bare environment
- how execution enters C from assembly
- how memory is laid out
- how the kernel initializes hardware
- how interrupts are managed
- how the system moves from boot to a running environment

This makes it a very useful project for understanding the foundations of operating systems.

## Current status and roadmap

This is still an educational kernel in active development. It has already reached the stage where the kernel boots, initializes a basic console, configures the GDT and IDT, starts the timer and keyboard, and manages simple page allocation.

The next likely steps are:

- improve the command parser
- add more system calls and kernel utilities
- expand interrupt handling
- implement paging and virtual memory
- add a proper task or process model
- build more drivers for hardware devices
- create a real shell or user-space environment

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Final note

firstOS is not trying to be a complete OS. It is a clear, low-level example of how an operating system kernel starts, initializes itself, and begins handling basic hardware. That makes it a great project for anyone who wants to understand the actual machinery behind the operating systems we use every day.

If you are learning kernel development, the best way to use this project is to read the code while it runs in QEMU, change a small piece, and observe how the system responds. That hands-on habit is where the real learning happens.
