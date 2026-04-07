/*  Linker Script  */
/* kernel.ld — tells linker where to put code */

ENTRY(_start)

SECTIONS {
    /* kernel loads at 0x40000 in QEMU virt */
    . = 0x40000;

    /* code section */
    .text : {
        *(.text.boot)   /* boot code first! */
        *(.text*)
    }

    /* read-only data */
    .rodata : {
        *(.rodata*)
    }

    /* initialized data */
    .data : {
        *(.data*)
    }

    /* uninitialized data (BSS) */
    __bss_start = .;
    .bss : {
        *(.bss*)
    }
    __bss_end = .;
}