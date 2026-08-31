# firstOS

A hand-written x86 hobby kernel. Multiboot1, freestanding C.

## Build

```bash
make          # build firstos.bin
make run      # boot in QEMU
make iso      # build bootable ISO (needs grub-mkrescue)
make clean
```

Requires a cross-compiler: `i686-elf-gcc` / `i686-elf-as`.

## License

MIT — see [LICENSE](LICENSE).
