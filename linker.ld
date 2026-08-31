ENTRY(_start)

SECTIONS
{
    /*
     * FirstOS kernel starts at 1 MiB.
     */
    . = 1M;

    /*
     * Beginning of kernel image.
     */
    __kernel_start = .;


    /*
     * Multiboot header.
     */
    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    }


    /*
     * Executable code.
     */
    .text ALIGN(4K) :
    {
        *(.text)
    }


    /*
     * Read-only data.
     */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }


    /*
     * Initialized data.
     */
    .data ALIGN(4K) :
    {
        *(.data)
    }


    /*
     * Uninitialized data / BSS.
     *
     * PMM bitmap lives here.
     */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }


    /*
     * End of kernel image.
     */
    __kernel_end = .;


    /*
     * Discard metadata that is not needed
     * by the kernel image.
     */
    /DISCARD/ :
    {
        *(.note.gnu.property)
        *(.comment)
        *(.eh_frame)
    }
}