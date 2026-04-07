# Kernel Memory & Scheduling - Step-by-Step Explanation

A detailed walkthrough of how **memory management** and **scheduling (interrupts)** are implemented in your ARM64 bare metal kernel across phases 2 and 3.

---

## Table of Contents

1. [Overview: Three Phases](#overview)
2. [Phase 2: Memory Management](#phase-2-memory-management)
   - [Physical Memory Manager (PMM)](#physical-memory-manager-pmm)
   - [Heap Allocator (kmalloc)](#heap-allocator-kmalloc)
   - [Page Tables & MMU](#page-tables--mmu)
3. [Phase 3: Scheduling & Interrupts](#phase-3-scheduling--interrupts)
   - [Exception Vector Table](#exception-vector-table)
   - [Timer Interrupts](#timer-interrupts)
   - [Interrupt Execution Flow](#interrupt-execution-flow)
4. [Memory Layout in RAM](#memory-layout-in-ram)

---

## Overview

Your kernel development follows a natural progression:

- **Phase 1**: Basic boot, UART output, memory initialization
- **Phase 2**: Physical memory tracking, heap allocation, virtual memory via MMU
- **Phase 3**: Exception handling, timer interrupts, interrupt-based scheduling infrastructure

This document focuses on **Phases 2 & 3**.

---

# Phase 2: Memory Management

## Physical Memory Manager (PMM)

### What It Does

The PMM tracks which 4KB "pages" of physical RAM are free or in use. Think of it as a librarian keeping track of which books are available.

### The Bitmap Approach (From `memory.c`)

```c
/* Each bit represents one 4KB page */
#define MAX_PAGES   (RAM_SIZE / PAGE_SIZE)  /* 128 MB / 4KB = 32,768 pages */
#define BITMAP_SIZE (MAX_PAGES / 64)        /* 512 u64 values */

static u64 bitmap[BITMAP_SIZE];             /* Storage for bits */
static u64 free_pages = 0;                  /* Counter of free pages */
```

**How the bitmap works:**
- Each `u64` holds 64 bits (one bit per page)
- `bitmap[0]` tracks pages 0-63
- `bitmap[1]` tracks pages 64-127
- And so on...

**Bit meanings:**
- **Bit = 0** → Page is FREE (available to allocate)
- **Bit = 1** → Page is USED (allocated, don't use it)

### PMM Step-by-Step: Initialization

From `kernel.c`:

```c
/* STEP 2: Initialize physical memory manager */
unsigned long free_start = PAGE_ALIGN((unsigned long)&__end);
pmm_init(free_start, RAM_END);
```

Here's what happens **inside `pmm_init()`**:

**Step 1: Mark ALL pages as used initially**
```c
void pmm_init(u64 start, u64 end) {
    /* Mark everything as used (safe default) */
    for (u64 i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0xFFFFFFFFFFFFFFFFUL;  /* All bits = 1 (all used) */
    }
    free_pages = 0;
```

Why? We want to explicitly mark only the free RAM as available, not accidentally use kernel space!

**Step 2: Mark the free range as available**
```c
    /* Calculate which pages are in the free range */
    u64 page_start = PHYS_TO_PAGE(PAGE_ALIGN(start) - RAM_START);
    u64 page_end   = PHYS_TO_PAGE((end & PAGE_MASK) - RAM_START);

    /* Clear bits for pages we can allocate */
    for (u64 p = page_start; p < page_end && p < MAX_PAGES; p++) {
        bit_clear(p);  /* Set bit = 0 (page is free) */
        free_pages++;
    }
    
    uart_puts("pmm: free pages = ");
    uart_putdec(free_pages);  /* e.g., 32,000 pages */
    uart_puts("\n");
}
```

**Result:** Bitmap now shows which pages (after kernel) are free.

### PMM Step-by-Step: Allocating a Page

From `kernel.c`:

```c
/* STEP 3: Test PMM - allocate 3 pages */
u64 p1 = pmm_alloc_page();
u64 p2 = pmm_alloc_page();
u64 p3 = pmm_alloc_page();
```

**Inside `pmm_alloc_page()`:**

```c
u64 pmm_alloc_page(void) {
    /* Scan bitmap for first free page */
    for (u64 p = 0; p < MAX_PAGES; p++) {
        if (!bit_test(p)) {        /* Is this page free? (bit = 0) */
            bit_set(p);            /* Mark it as used (bit = 1) */
            free_pages--;          /* One less free page */
            return RAM_START + PAGE_TO_PHYS(p); /* Return physical address */
        }
    }
    uart_puts("pmm: OUT OF MEMORY!\n");
    return 0;
}
```

**Example:**
1. First call: Scan finds page #N free → Set bit[N] = 1 → Return `0x40000000 + (N × 4096)`
2. Second call: Scan finds page #(N+1) free → Set bit[N+1] = 1 → Return address of next page
3. Pages are allocated **sequentially** (boring but simple!)

### PMM Step-by-Step: Freeing a Page

```c
/* STEP 3b: Test PMM - free then reallocate */
pmm_free_page(p2);    /* Free page p2 */
u64 p4 = pmm_alloc_page();  /* Should get p2 back */
```

**Inside `pmm_free_page()`:**

```c
void pmm_free_page(u64 addr) {
    /* Calculate which page this address represents */
    u64 page = PHYS_TO_PAGE(addr - RAM_START);
    bit_clear(page);      /* Mark bit = 0 (page is free) */
    free_pages++;         /* Count it as free */
}
```

**Result:** `p4` will equal `p2` because that page is now available again!

---

## Heap Allocator (kmalloc)

### What It Does

While PMM allocates whole **4KB pages**, `kmalloc()` allocates **arbitrary-sized chunks** from a larger memory region (the "heap"). This is like renting office space: PMM buys whole buildings, kmalloc subdivides them!

### Simple Bump Allocator Design

From `memory.c`:

```c
static u64 heap_start = 0;
static u64 heap_ptr   = 0;   /* "Bump" pointer - points to next free location */
static u64 heap_end   = 0;

void heap_init(void) {
    /* Start heap after kernel code/data */
    heap_start = PAGE_ALIGN((u64)&__end);
    heap_ptr = heap_start;
    heap_end = heap_start + (1024UL * 1024UL);  /* 1MB heap to start */
    
    uart_puts("heap: start = ");
    uart_puthex(heap_start);  /* e.g., 0x40100000 */
    uart_puts("\n");
}
```

### How kmalloc Works

From `kernel.c`:

```c
/* STEP 4: Allocate from heap with kmalloc */
char* buf1 = (char*)kmalloc(64);  
char* buf2 = (char*)kmalloc(128);
char* buf3 = (char*)kmalloc(256);
```

**Inside `kmalloc()`:**

```c
void* kmalloc(u64 size) {
    /* Align requested size to 8-byte boundary */
    size = (size + 7UL) & ~7UL;  /* (64 + 7) & ~7 = 64, (128+7) & ~7 = 128 */
    
    /* Check if there's enough space */
    if (heap_ptr + size > heap_end) {
        uart_puts("kmalloc: no space!\n");
        return 0;
    }
    
    /* Record current pointer as the allocated address */
    void* ptr = (void*)heap_ptr;
    
    /* Bump pointer forward for next allocation */
    heap_ptr += size;
    
    return ptr;
}
```

### Visual Example:

```
Initial state:
┌─────────────────────────────────────────┐
│ (free heap space)                       │
│ heap_ptr ↓                              │
└─────────────────────────────────────────┘
0x40100000                          0x40200000 (end)

After buf1 = kmalloc(64):
┌────────┬──────────────────────────────────┐
│ buf1   │ (free heap space)                │
│64 bytes│ heap_ptr ↓                       │
└────────┴──────────────────────────────────┘
0x40100000

After buf2 = kmalloc(128):
┌────────┬─────────────┬───────────────────┐
│ buf1   │ buf2        │ (free heap space) │
│64      │ 128 bytes   │ heap_ptr ↓        │
└────────┴─────────────┴───────────────────┘

After buf3 = kmalloc(256):
┌────────┬─────────────┬──────────┬────┐
│ buf1   │ buf2        │ buf3     │free│
│64      │ 128         │ 256 bytes│    │
└────────┴─────────────┴──────────┴────┘
                              heap_ptr ↓
```

**Key characteristic:** This is a **bump allocator** - very simple, very fast, but can't free individual allocations! Once allocated, a chunk stays allocated until we reset the entire heap.

---

## Page Tables & MMU

### What It Does

While PMM and kmalloc work with **physical addresses** (real RAM), processes need **virtual addresses** (isolated memory spaces). The MMU (Memory Management Unit) maps virtual →  physical addresses using **page tables**.

### ARM64 Page Table Structure

From `memory.c`:

```c
/* ARM64 uses 4-level hierarchical page tables
   VA bits:
     [47:39] = L0 index (9 bits - points to L1 table)
     [38:30] = L1 index (9 bits - points to blocks or L2 table)
     [29:21] = L2 index (9 bits)
     [20:12] = L3 index (9 bits - points to page)
     [11:0]  = page offset (12 bits - offset within 4KB page)
*/

/* L1 page table - 512 entries, each entry = 8 bytes */
static u64 l1_table[512] __attribute__((aligned(4096)));
```

**Why hierarchical?**
- A flat table for 47-bit addresses would need $2^{47}$ entries! (Too much memory)
- Hierarchy means we only allocate tables for populated regions
- Each table: 512 entries × 8 bytes = 4KB (exactly one page!)

### Page Table Entry (PTE) Format

```c
#define PTE_VALID      (1UL << 0)   /* Bit 0: Entry is valid */
#define PTE_TABLE      (1UL << 1)   /* Bit 1: Points to next level (1) or block (0) */
#define PTE_AF         (1UL << 10)  /* Bit 10: Access flag (must be set!) */
#define PTE_ATTR_MEM   (0UL << 2)   /* Memory is normal cacheable memory */
#define PTE_AP_RW      (0UL << 6)   /* Read-Write access permission */
```

Example entry for a 1GB block:
```
l1_table[0] = 0x40000000  /* Physical address of memory */
            | PTE_VALID   /* This entry is valid (bit 0 = 1) */
            | PTE_BLOCK   /* This is a block mapping (bit 1 = 0) */
            | PTE_AF      /* Access flag (bit 10 = 1) */
            | PTE_ATTR_MEM
            | PTE_AP_RW;
```

### MMU Initialization

From `kernel.c`:

```c
/* STEP 5: Setup MMU - identity map first 1GB of RAM */
mmu_init();

/* STEP 6: After MMU enabled, memory still works! */
uart_puts("UART still works! (good!)\n");
buf1[0]='O'; buf1[1]='K';  /* Can still access memory */
```

**What "identity mapping" means:** Virtual address = Physical address. So the CPU doesn't care about translation; things work as before. BUT the infrastructure is in place for more complex mappings later!

---

# Phase 3: Scheduling & Interrupts

Now we add **real-time scheduling** via timer interrupts. Instead of the kernel running linearly, it's interrupted regularly by the hardware timer, allowing it to context-switch between processes.

## Exception Vector Table

### What It Does

When the CPU encounters an exception (timer, fault, etc.), it doesn't know what to do. We have to tell it where to find the handler code via the **exception vector table**.

### The Four Exception Types (ARM64)

From `exceptions.h`:

```c
#define EXC_SYNC    0  /* Synchronous: divide by zero, invalid instruction */
#define EXC_IRQ     1  /* Interrupt: timer, UART, keyboard input */
#define EXC_FIQ     2  /* Fast Interrupt: rarely used */
#define EXC_SERROR  3  /* System Error: cache errors, etc. */
```

### Vector Table Layout (From `exceptions.c`)

```
Exception Vector Table (2KB total, 2^11 aligned):

Offset   Entry Type                 Our Implementation
───────  ─────────────────────────  ──────────────────
0x000    Current EL (SP0) Sync      BRK (unimplemented)
0x080    Current EL (SP0) IRQ       BRK (unimplemented)
0x100    Current EL (SP0) FIQ       BRK (unimplemented)
0x180    Current EL (SP0) SError    BRK (unimplemented)

0x200    Current EL (SPx) Sync      → exc_sync_handler() ← WE USE THIS!
0x280    Current EL (SPx) IRQ       → exc_irq_handler() ← TIMER GOES HERE!
0x300    Current EL (SPx) FIQ       → exc_fiq_handler()
0x380    Current EL (SPx) SError    → exc_serror_handler()

0x400+   Lower EL (user mode) entries...
```

**Key insight:** SPx = Stack Pointer 1 (kernel stack), so when an exception happens at kernel privilege level, we use the kernel stack, not the regular stack.

### Installing the Vector Table

From `exceptions.c`:

```c
void exceptions_init(void) {
    uart_puts("Installing exception vector table...\n");
    
    /* STEP 1: Get address of our vector table */
    u64 vbar_addr = (u64)&exception_vector_table;
    
    /* STEP 2: Write address to VBAR_EL1 register
       VBAR = Vector Base Address Register
       This tells the CPU: "When exceptions happen, jump to this address!"
    */
    asm volatile("msr vbar_el1, %0" : : "r"(vbar_addr));
    
    /* STEP 3: Verify it was set correctly */
    u64 vbar_check;
    asm volatile("mrs %0, vbar_el1" : "=r"(vbar_check));
    uart_puts("VBAR_EL1 = ");
    uart_puthex(vbar_check);
    uart_puts("\n");
}
```

**What happens when an exception occurs:**
1. CPU reads the VBAR_EL1 register
2. Adds an offset based on exception type (0x200 for sync at EL1, 0x280 for IRQ, etc.)
3. Jumps to that address and starts executing code

---

## Timer Interrupts

### How the ARM Generic Timer Works

From `exceptions.c`:

```c
void timer_init(void) {
    /* ARM Generic Timer has:
       - CNTFRQ_EL0: Clock frequency (how fast the counter ticks)
       - CNTVCT_EL0: Virtual counter (increments each clock cycle)
       - CNTV_TVAL_EL0: "Timeout" value (causes interrupt when counter reaches it)
       - CNTV_CTL_EL0: Timer control (enable/disable)
    */
    
    /* STEP 1: Read the timer frequency from the hardware */
    u64 freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    uart_puts("Timer frequency: ");
    uart_putdec(freq);  /* e.g., 62,500,000 Hz on QEMU */
    uart_puts(" Hz\n");
    
    /* STEP 2: Calculate ticks per millisecond */
    u64 ticks_per_ms = freq / 1000;  /* 62,500 ticks per 1ms */
    timer_ticks_per_ms = ticks_per_ms;  /* Save for IRQ handler */
    uart_puts("Ticks per 1ms: ");
    uart_putdec(ticks_per_ms);
    uart_puts("\n");
    
    /* STEP 3: Set initial timeout to 1 millisecond */
    asm volatile("msr cntv_tval_el0, %0" : : "r"(ticks_per_ms));
    
    /* STEP 4: Enable the timer
       CNTV_CTL_EL0 bit 0 = 1 means "timer enabled"
       CNTV_CTL_EL0 bit 1 = 0 means "interrupt not masked (enabled)"
    */
    u32 ctl = 1;
    asm volatile("msr cntv_ctl_el0, %0" : : "r"(ctl));
    
    uart_puts("Timer initialized for 1ms interrupts\n");
}
```

### Enabling Interrupts at CPU Level

From `exceptions.h`:

```c
static inline void enable_interrupts(void) {
    /* PSTATE = Processor State register
       Bit 7 = D (Debug exception disable)
       Bit 6 = A (SError disable)
       Bit 5 = I (IRQ disable) ← We care about this!
       Bit 4 = F (FIQ disable)
       
       daifclr = "disable-all-interrupt-flags clear"
       #2 = Clear bit 5 (the I bit)
    */
    asm volatile("msr daifclr, #2" ::: "memory");
}
```

**Key point:** By default, all interrupts are disabled for safety. We must explicitly enable them!

---

## Interrupt Execution Flow

Let's trace what happens when the timer fires:

### Tick 0: Before Timer Fires

```
kernel_main() is running
...
exceptions_init()  ← Sets up vector table and enables timer
enable_interrupts() ← Clears the IRQ disable bit in PSTATE
(kernel continues running)
```

### Tick 1: Timer Fires (1 ms later)

```
1. CPU hardware counts: CNTVCT_EL0 increments every clock cycle
2. When CNTVCT_EL0 == CNTV_TVAL_EL0 (1,000,000 clocks), interrupt fires
3. CPU checks PSTATE.I bit - it's clear (interrupts enabled!)
4. CPU performs exception entry (saves state, switches to EL1, etc.)
5. CPU reads VBAR_EL1 to find exception vector table
6. CPU adds offset 0x280 (for IRQ at EL1) to get handler address
7. CPU jumps to exception_vector_table + 0x280
```

### The IRQ Vector Handler (Assembly, From `exceptions.c`)

```asm
/* 0x280: IRQ Handler Entry Point */
    str x30, [sp, #-16]!    /* Save return register (x30/lr) on kernel stack */
    bl exc_irq_handler      /* Call C function handler */
    ldr x30, [sp], #16      /* Restore return register */
    eret                    /* Return from exception (resume interrupted code) */
```

**What each instruction does:**
- `str x30, [sp, #-16]!` — Save the link register (return address) by pushing it on the kernel stack
- `bl exc_irq_handler` — Branch and Link to our C interrupt handler function
- `ldr x30, [sp], #16` — Restore the link register
- `eret` — Exception Return: resume whatever was running before

### The IRQ Handler in C (From `exceptions.c`)

```c
void exc_irq_handler(void) {
    /* STEP 1: Increment tick counter */
    tick_count++;
    
    /* STEP 2: Reload timer for next 1ms interrupt
       This also automatically clears the current interrupt
    */
    asm volatile("msr cntv_tval_el0, %0" : : "r"(timer_ticks_per_ms));
    
    /* STEP 3: Print progress indicator */
    if (tick_count % 1000 == 0) {
        uart_puts(".");  /* Print a dot every second */
    }
    
    /* Handler ends, assembly code above does `eret` to resume */
}
```

### Tick 2: Return to Normal Execution

```
1. exc_irq_handler() finishes
2. Assembly code at 0x280 executes `eret`
3. CPU restores state, returns to kernel_main() on the next instruction
4. Kernel continues running from where it left off
5. (Meanwhile, timer is set up to fire again in 1ms...)
```

---

## Complete Interrupt Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│  kernel_main() running...                                   │
│  ... (kernel code executing)                                │
│  exceptions_init() → timer_init() → enable_interrupts()    │
│  ... (kernel continues)                                     │
│                                                              │
│  [TIMER FIRES at 1ms boundary]                              │
│         ↓                                                    │
│  CPU checks: Is interrupt enabled? YES (PSTATE.I = 0)      │
│         ↓                                                    │
│  CPU saves state (registers, return address)                │
│         ↓                                                    │
│  CPU reads VBAR_EL1 → gets 0x40040000 (vector table addr)  │
│  CPU adds offset 0x280 (IRQ handler) → 0x40040280          │
│         ↓                                                    │
│  CPU jumps to exception_vector_table + 0x280               │
│         ↓                                                    │
│  Assembly code: "str x30, [sp, #-16]!" (save return addr)  │
│  Assembly code: "bl exc_irq_handler" (call C handler)      │
│         ↓                                                    │
│  C code in exc_irq_handler():                               │
│    ├─ tick_count++ (now 1)                                  │
│    ├─ cntv_tval_el0 = ticks_per_ms (reload timer)          │
│    └─ (optional) print progress                            │
│         ↓                                                    │
│  Assembly code: "ldr x30, [sp], #16" (restore return addr) │
│  Assembly code: "eret" (exit exception, resume kernel)     │
│         ↓                                                    │
│  CPU resumes kernel_main() from next instruction            │
│  ... (kernel continues, unaware of interruption)           │
│                                                              │
│  [1ms passes...]                                            │
│  [TIMER FIRES AGAIN]                                       │
│  (repeat entire flow)                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Testing the Interrupt System

From `kernel.c`:

```c
uart_puts("Initial tick count: ");
uart_putdec(get_tick_count());  /* Should be 0 */
uart_puts("\n");

/* Spin loop - CPU just runs in circles, no special work */
u64 loop_count = 0;
u64 max_iterations = 100000000;
while(loop_count < max_iterations) {
    loop_count++;
    /* Meanwhile, every 1ms the timer interrupt fires
       and increments tick_count in the background! */
}

uart_puts("Final tick count: ");
uart_putdec(get_tick_count());  /* Should be ~100,000 if no timer support */
                                /* Or higher if timer works */
```

**Key insight:** While the CPU is spinning in the loop, the timer continues firing in the background. Each interrupt is transparent to the main code - it runs, updates tick_count, and returns, all without the loop noticing!

---

# Memory Layout in RAM

Here's the complete memory map after all phases:

```
┌─────────────────────────────────────────────────────────┐
│  0x40000000  KERNEL CODE & DATA                          │
│              (loaded by bootloader)                      │
│  0x40010000  ← kernel code ends (__end)                 │
├─────────────────────────────────────────────────────────┤
│  0x40010000  HEAP (kmalloc)                              │
│              1 MB allocated                              │
│  0x40110000  ← heap end                                  │
├─────────────────────────────────────────────────────────┤
│  0x40110000  FREE PHYSICAL PAGES (managed by PMM)        │
│              ~32,000 pages × 4KB = ~128 MB               │
│                                                          │
│              (These 4KB pages are allocated and freed    │
│               by pmm_alloc_page() / pmm_free_page())     │
│                                                          │
│  0x48000000  ← RAM end (128 MB total)                   │
├─────────────────────────────────────────────────────────┤
│  0x09000000  UART0 (serial port - memory-mapped I/O)    │
│  0x09080000  SYSCON (power management)                  │
│              + Other hardware peripherals                │
└─────────────────────────────────────────────────────────┘
```

---

# Summary: How It All Works Together

## Memory Management Summary (Phase 2)

1. **Physical Memory Manager (PMM):**
   - Uses a bitmap (each bit = one 4KB page)
   - `pmm_alloc_page()` finds free pages, marks them used
   - `pmm_free_page()` marks pages as free again
   - Simple but effective for page-level allocation

2. **Heap Allocator (kmalloc):**
   - Allocates variable-sized chunks from a 1MB heap region
   - Uses a simple bump allocator (fast, no fragmentation, no freeing)
   - Sits on top of PMM (future: could expand heap using pmm_alloc_page())

3. **MMU/Page Tables:**
   - ARM64 uses 4-level hierarchical page tables
   - Maps virtual addresses → physical addresses
   - Currently identity-mapped (VA = PA), but infrastructure ready for complex mappings

## Scheduling & Interrupts Summary (Phase 3)

1. **Exception Vector Table:**
   - Maps exception types to handler functions
   - Installed via VBAR_EL1 register
   - 16 total entries (4 types × 4 privilege levels)

2. **Timer Interrupts:**
   - ARM Generic Timer hardware counts at frequency (e.g., 62.5 MHz)
   - Timer fires every 1ms, causing IRQ exception
   - IRQ handler increments tick counter, resets timer

3. **Interrupt Handling Flow:**
   - CPU jumps to vector table offset based on exception type
   - Calls appropriate handler function
   - Handler does its work (e.g., increment tick count)
   - Handler returns via `eret` instruction
   - CPU resumes interrupted code seamlessly

---

## Why This Design?

**For Memory Management:**
- PMM's bitmap is simple, cache-friendly, and doesn't require dynamic allocation itself
- Bump allocator is extreme simple for quick prototyping
- Hierarchical page tables allow sparse address spaces

**For Scheduling:**
- Timer interrupts provide a predictable, hardware-backed way to regain control
- The vector table abstraction cleanly separates CPU mechanics from kernel code
- `tick_count` provides a fair clock for round-robin scheduling of future processes

---

## Next Steps (Phase 4)

With this foundation, Phase 4 can implement:
1. **Process structures** - Define `struct process { registers, memory, state, ... }`
2. **Process list** - Keep track of ready/blocked processes
3. **Context switching** - In the IRQ handler: save current process state, restore next process state
4. **Scheduler** - Simple priority or round-robin to choose which process runs next

The interrupt mechanism is already in place; we just need process management on top of it!

---

*Last Updated: Phase 3 (Scheduling & Interrupts)*
*Architecture: ARM64 (AArch64) Bare Metal*
*Target Platform: QEMU virt machine*
