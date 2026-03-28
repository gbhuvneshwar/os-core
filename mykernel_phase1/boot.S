/* Boot Assembly */

/* boot.S — first code CPU runs! */
/* ARM64 assembly */

.section ".text.boot"
.global _start

_start:
    /* set up stack pointer */
    /* stack grows DOWN from 0x80000 */
    ldr x30, =0x80000
    mov sp, x30

    /* clear BSS section (zero out globals) */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, #0
clear_bss:
    cmp x0, x1
    bge done_clear
    str x2, [x0], #8    /* store 0, advance 8 bytes */
    b clear_bss
done_clear:

    /* jump to C kernel! */
    bl kernel_main

    /* if kernel_main returns: hang forever */
hang:
    wfe             /* wait for event (low power) */
    b hang