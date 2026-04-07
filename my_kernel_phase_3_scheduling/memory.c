#include "memory.h"
#include "uart.h"

/* ─────────────────────────────────────────
   PHYSICAL MEMORY MANAGER
   tracks which 4KB pages are free/used
   uses a simple bitmap!

   bitmap: each BIT = one 4KB page
           bit=0 → page FREE
           bit=1 → page USED

   connecting to our discussion:
     physical frames we discussed!
     each frame = 4KB
     MMU maps virtual pages to these!
   ───────────────────────────────────────── */

#define MAX_PAGES   (RAM_SIZE / PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 64)

static u64 bitmap[BITMAP_SIZE];
static u64 free_pages = 0;
static u64 phys_start = 0;

/* set bit = mark page as USED */
static void bit_set(u64 page) {
    bitmap[page / 64] |= (1UL << (page % 64));
}

/* clear bit = mark page as FREE */
static void bit_clear(u64 page) {
    bitmap[page / 64] &= ~(1UL << (page % 64));
}

/* test bit = is page used? */
static int bit_test(u64 page) {
    return (bitmap[page / 64] >>
            (page % 64)) & 1;
}

void pmm_init(u64 start, u64 end) {
    phys_start = RAM_START;

    /* mark ALL pages as used first */
    for (u64 i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0xFFFFFFFFFFFFFFFFUL;
    }
    free_pages = 0;

    /* free pages in range start..end */
    u64 page_start = PHYS_TO_PAGE(
        PAGE_ALIGN(start) - RAM_START);
    u64 page_end   = PHYS_TO_PAGE(
        (end & PAGE_MASK) - RAM_START);

    for (u64 p = page_start;
         p < page_end && p < MAX_PAGES; p++) {
        bit_clear(p);
        free_pages++;
    }

    uart_puts("pmm: init done\n");
    uart_puts("pmm: free pages = ");
    uart_putdec(free_pages);
    uart_puts(" (");
    uart_putdec(free_pages * 4);
    uart_puts(" KB)\n");
}

/* allocate ONE physical page */
/* returns physical address */
u64 pmm_alloc_page(void) {
    for (u64 p = 0; p < MAX_PAGES; p++) {
        if (!bit_test(p)) {
            bit_set(p);
            free_pages--;
            /* return physical address */
            return RAM_START +
                   PAGE_TO_PHYS(p);
        }
    }
    uart_puts("pmm: OUT OF MEMORY!\n");
    return 0;
}

/* free ONE physical page */
void pmm_free_page(u64 addr) {
    u64 page = PHYS_TO_PAGE(
        addr - RAM_START);
    bit_clear(page);
    free_pages++;
}

u64 pmm_free_count(void) {
    return free_pages;
}

/* ─────────────────────────────────────────
   HEAP ALLOCATOR (kmalloc)
   simple bump allocator
   uses pmm to get pages
   ───────────────────────────────────────── */
static u64 heap_start = 0;
static u64 heap_ptr   = 0;
static u64 heap_end   = 0;

/* symbols from linker script */
extern u64 __end;

void heap_init(void) {
    /* heap starts after kernel */
    heap_start = PAGE_ALIGN(
        (u64)&__end);
    /* give heap 1MB to start */
    heap_ptr = heap_start;
    heap_end = heap_start +
               (1024UL * 1024UL);

    uart_puts("heap: start = ");
    uart_puthex(heap_start);
    uart_puts("\n");
    uart_puts("heap: end   = ");
    uart_puthex(heap_end);
    uart_puts("\n");
}

void* kmalloc(u64 size) {
    /* align to 8 bytes */
    size = (size + 7UL) & ~7UL;

    if (heap_ptr + size > heap_end) {
        uart_puts("kmalloc: no space!\n");
        return 0;
    }

    void* ptr = (void*)heap_ptr;
    heap_ptr += size;
    return ptr;
}

/* ─────────────────────────────────────────
   PAGE TABLE SETUP (ARM64 MMU)

   ARM64 uses 4-level page tables:
   VA bits:
     [63:48] = not used (must be 0 or 1)
     [47:39] = L0 index (9 bits)
     [38:30] = L1 index (9 bits)
     [29:21] = L2 index (9 bits)
     [20:12] = L3 index (9 bits)
     [11:0]  = page offset (12 bits)

   we use 1GB blocks (L1 mapping)
   simpler! one L1 entry = 1GB!

   page table entry bits:
     bit 0:    valid (1 = entry valid)
     bit 1:    type  (1 = table/page,
                      0 = block)
     bits 7:6: AP    (access permission)
     bit 10:   AF    (access flag, must set!)
     bits 51:48: physical address bits
   ───────────────────────────────────────── */

/* L1 page table */
/* must be 4096 byte aligned! */
/* 512 entries × 8 bytes = 4096 bytes */
static u64 l1_table[512]
    __attribute__((aligned(4096)));

/* page table entry flags */
#define PTE_VALID      (1UL << 0)
#define PTE_TABLE      (1UL << 1)
#define PTE_AF         (1UL << 10)
#define PTE_SH_INNER   (3UL << 8)
#define PTE_BLOCK      (0UL << 1)

/* memory attributes index */
#define PTE_ATTR_MEM   (0UL << 2)  /* normal memory */
#define PTE_ATTR_DEV   (1UL << 2)  /* device memory */

/* access permissions */
#define PTE_AP_RW      (0UL << 6)  /* read/write */
#define PTE_AP_RO      (1UL << 6)  /* read only */

/* ARM64 system registers */
#define TCR_T0SZ       (64 - 39)   /* 512GB VA space */
#define TCR_TG0_4K     (0UL << 14) /* 4KB granule */
#define TCR_SH0_INNER  (3UL << 12) /* inner shareable */
#define TCR_ORGN0_WBWA (1UL << 10) /* outer WB WA */
#define TCR_IRGN0_WBWA (1UL << 8)  /* inner WB WA */
#define TCR_IPS_32BIT  (0UL << 32) /* 32-bit PA */

/* MAIR - memory attribute indirection register */
/* index 0 = normal memory */
/* index 1 = device memory (nGnRnE) */
#define MAIR_VAL \
    ((0xFFUL << 0) | (0x00UL << 8))

extern void __dsb_sy(void);
extern void __isb(void);

void mmu_init(void) {
    uart_puts("\n--- MMU SETUP ---\n");

    /* ── STEP 1: create page table ── */
    uart_puts("step 1: creating page table\n");

    /* clear page table */
    for (int i = 0; i < 512; i++) {
        l1_table[i] = 0;
    }

    /*
     * map memory using 1GB blocks
     *
     * virtual address = physical address
     * (identity mapping = simplest!)
     * VA 0x40000000 → PA 0x40000000
     *
     * L1 index for 0x40000000:
     *   bits[38:30] of 0x40000000
     *   = 0x40000000 >> 30 = 1
     *   so l1_table[1] maps 0x40000000
     */

    /* map 0x00000000 - 0x40000000 */
    /* devices (UART etc) live here */
    l1_table[0] =
        0x00000000UL |   /* physical addr */
        PTE_VALID    |   /* valid entry */
        PTE_AF       |   /* access flag */
        PTE_SH_INNER |   /* shareable */
        PTE_ATTR_DEV |   /* device memory! */
        PTE_BLOCK;       /* 1GB block */

    uart_puts("  L1[0] = ");
    uart_puthex(l1_table[0]);
    uart_puts(" (device 0x00000000)\n");

    /* map 0x40000000 - 0x80000000 */
    /* RAM lives here */
    l1_table[1] =
        0x40000000UL |   /* physical addr */
        PTE_VALID    |   /* valid entry */
        PTE_AF       |   /* access flag */
        PTE_SH_INNER |   /* shareable */
        PTE_ATTR_MEM |   /* normal memory! */
        PTE_BLOCK;       /* 1GB block */

    uart_puts("  L1[1] = ");
    uart_puthex(l1_table[1]);
    uart_puts(" (RAM 0x40000000)\n");

    /* ── STEP 2: set MAIR register ── */
    uart_puts("step 2: setting MAIR\n");
    __asm__ volatile(
        "msr mair_el1, %0"
        :: "r"(MAIR_VAL)
    );

    /* ── STEP 3: set TCR register ── */
    uart_puts("step 3: setting TCR\n");
    u64 tcr = TCR_T0SZ      |
              TCR_TG0_4K    |
              TCR_SH0_INNER |
              TCR_ORGN0_WBWA|
              TCR_IRGN0_WBWA|
              TCR_IPS_32BIT;
    __asm__ volatile(
        "msr tcr_el1, %0"
        :: "r"(tcr)
    );

    /* ── STEP 4: set TTBR0 ── */
    /* TTBR0 = page table base address */
    uart_puts("step 4: setting TTBR0\n");
    uart_puts("  page table at: ");
    uart_puthex((u64)l1_table);
    uart_puts("\n");

    __asm__ volatile(
        "msr ttbr0_el1, %0"
        :: "r"((u64)l1_table)
    );

    /* ── STEP 5: sync ── */
    __dsb_sy();
    __isb();

    /* ── STEP 6: enable MMU ── */
    uart_puts("step 5: enabling MMU...\n");

    u64 sctlr;
    __asm__ volatile(
        "mrs %0, sctlr_el1"
        : "=r"(sctlr)
    );

    /* bit 0 = MMU enable */
    /* bit 2 = data cache enable */
    /* bit 12 = instruction cache enable */
    sctlr |= (1UL << 0);   /* MMU on! */
    sctlr |= (1UL << 2);   /* dcache */
    sctlr |= (1UL << 12);  /* icache */

    __asm__ volatile(
        "msr sctlr_el1, %0"
        :: "r"(sctlr)
    );

    /* must sync after MMU enable */
    __isb();

    uart_puts("MMU IS ON!\n");
    uart_puts("virtual memory active!\n");
    uart_puts("--- MMU SETUP DONE ---\n\n");
}
