# firstOS

A small x86 hobby operating system kernel written in C and assembly. It boots via Multiboot 1, initializes VGA text mode, and prints a simple startup banner.

## Overview

firstOS is a minimal kernel project focused on learning low-level OS development. The project currently includes:

- Multiboot 1 entry point
- 32-bit protected mode startup
- VGA text-mode output
- Simple kernel boot banner
- Makefile-based build flow

## Features

- Minimal 32-bit kernel entry
- Freestanding C code
- VGA text output driver
- QEMU boot support
- ISO build support via GRUB

## Requirements

To build and run this project, you will need:

- GCC
- GNU ld
- QEMU (`qemu-system-i386`)
- GRUB tools (`grub-mkrescue`) for ISO generation
- A 32-bit freestanding toolchain, such as `i686-elf-gcc` / `i686-elf-as`

## Build

```bash
make
```

This produces `firstos.bin`.

Run in QEMU:

```bash
make run
```

Build a bootable ISO:

```bash
make iso
```

Clean generated files:

```bash
make clean
```

## Project Structure

```text
firstos/
├── boot/
│   └── boot.s
├── include/
│   └── vga.h
├── kernel/
│   ├── kmain.c
│   └── vga.c
├── LICENSE
├── Makefile
├── README.md
├── linker.ld
└── .gitignore
```

## Current Status

This is an educational kernel and still under active development. It currently focuses on early boot and basic console output, with future work planned for:

- GDT and IDT setup
- Memory management
- Paging
- User-space preparation
- Keyboard and interrupt handling

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
