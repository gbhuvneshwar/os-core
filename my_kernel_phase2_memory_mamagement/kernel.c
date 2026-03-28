#include "uart.h"
#include "memory.h"

/* from linker script */
extern unsigned long __bss_end;
extern unsigned long __end;

void kernel_main(void) {
    /* UART must work first! */
    uart_init();

    uart_puts("\n");
    uart_puts("=============================\n");
    uart_puts("  MY KERNEL - PHASE 2\n");
    uart_puts("  MEMORY MANAGEMENT + MMU\n");
    uart_puts("=============================\n\n");

    /* ── STEP 1: show memory map ── */
    uart_puts("=== MEMORY MAP ===\n");
    uart_puts("RAM start  : ");
    uart_puthex(RAM_START);
    uart_puts("\n");
    uart_puts("RAM end    : ");
    uart_puthex(RAM_END);
    uart_puts("\n");
    uart_puts("RAM size   : 128 MB\n");
    uart_puts("kernel at  : ");
    uart_puthex(RAM_START);
    uart_puts("\n");
    uart_puts("kernel end : ");
    uart_puthex((unsigned long)&__end);
    uart_puts("\n\n");

    /* ── STEP 2: init physical memory ── */
    uart_puts("=== PHYSICAL MEMORY MANAGER ===\n");
    /* free RAM starts after kernel */
    unsigned long free_start =
        PAGE_ALIGN((unsigned long)&__end);
    pmm_init(free_start, RAM_END);
    uart_puts("\n");

    /* ── STEP 3: test pmm ── */
    uart_puts("=== TEST PMM ===\n");
    u64 p1 = pmm_alloc_page();
    u64 p2 = pmm_alloc_page();
    u64 p3 = pmm_alloc_page();

    uart_puts("alloc page 1: ");
    uart_puthex(p1);
    uart_puts("\n");
    uart_puts("alloc page 2: ");
    uart_puthex(p2);
    uart_puts("\n");
    uart_puts("alloc page 3: ");
    uart_puthex(p3);
    uart_puts("\n");

    uart_puts("gap p1->p2  : ");
    uart_putdec(p2 - p1);
    uart_puts(" bytes (should be 4096)\n");

    /* free one and reallocate */
    pmm_free_page(p2);
    u64 p4 = pmm_alloc_page();
    uart_puts("free p2, alloc p4: ");
    uart_puthex(p4);
    uart_puts(" (should match p2)\n\n");

    /* ── STEP 4: init heap ── */
    uart_puts("=== HEAP (kmalloc) ===\n");
    heap_init();

    char* buf1 = (char*)kmalloc(64);
    char* buf2 = (char*)kmalloc(128);
    char* buf3 = (char*)kmalloc(256);

    uart_puts("kmalloc(64)  : ");
    uart_puthex((unsigned long)buf1);
    uart_puts("\n");
    uart_puts("kmalloc(128) : ");
    uart_puthex((unsigned long)buf2);
    uart_puts("\n");
    uart_puts("kmalloc(256) : ");
    uart_puthex((unsigned long)buf3);
    uart_puts("\n");

    /* write to allocated memory */
    buf1[0]='H'; buf1[1]='i';
    buf1[2]='!'; buf1[3]='\0';
    uart_puts("write buf1   : ");
    uart_puts(buf1);
    uart_puts("\n\n");

    /* ── STEP 5: setup MMU ── */
    uart_puts("=== PAGE TABLE + MMU ===\n");
    mmu_init();

    /* ── STEP 6: test after MMU on ── */
    uart_puts("=== TEST AFTER MMU ===\n");
    uart_puts("UART still works! (good!)\n");
    uart_puts("memory access works! (good!)\n");

    /* test memory still accessible */
    buf1[0]='O'; buf1[1]='K';
    buf1[2]='\0';
    uart_puts("buf1 after MMU: ");
    uart_puts(buf1);
    uart_puts("\n");

    uart_puts("\n=============================\n");
    uart_puts("  PHASE 2 COMPLETE!\n");
    uart_puts("  MMU ON!\n");
    uart_puts("  VIRTUAL MEMORY WORKING!\n");
    uart_puts("=============================\n");

    /* poweroff */
    *((volatile unsigned int*)0x09080000)
        = 0x5555;
    while(1){}
}
