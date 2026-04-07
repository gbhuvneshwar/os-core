ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text :
    {
        KEEP(*(.text.boot))
        *(.text*)
    }

    . = ALIGN(4096);
    .rodata :
    {
        *(.rodata*)
    }

    . = ALIGN(4096);
    .data :
    {
        *(.data*)
    }

    . = ALIGN(4096);
    __bss_start = .;
    .bss :
    {
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(4096);
    __bss_end = .;

    . = ALIGN(4096);
    __end = .;
}
