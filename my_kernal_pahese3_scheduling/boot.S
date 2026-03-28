.section ".text.boot"
.global _start

_start:
    /* each CPU core has an ID */
    /* only run on core 0! */
    /* other cores hang */
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbnz x0, hang

    /* set stack pointer */
    /* just below kernel at 0x40000000 */
    ldr x0, =0x47FFF000  /* Safety margin below top */
    mov sp, x0

    /* zero out BSS section */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, xzr
zero_bss:
    cmp x0, x1
    b.ge done_bss
    str x2, [x0], #8
    b zero_bss
done_bss:

    /* jump to C code */
    bl kernel_main

hang:
    wfe
    b hang

.global __dsb_sy
__dsb_sy:
    dsb sy
    ret

.global __isb
__isb:
    isb
    ret
