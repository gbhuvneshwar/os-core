#include "uart.h"
#include "memory.h"
#include "exceptions.h"
#include "thread.h"

/* from linker script */
extern unsigned long __bss_end;
extern unsigned long __end;

/* ═══════════════════════════════════════════════════════════════
   GLOBAL CONTEXT SWITCH VARIABLES
   ═══════════════════════════════════════════════════════════════ */
volatile int context_switch_requested = 0;
volatile u32 next_thread_id = 0;

/* ═══════════════════════════════════════════════════════════════
   THREAD ENTRY POINTS
   
   Multiple threads running in ONE process.
   All threads share:
   - Code segment
   - Data segment  
   - Heap
   
   Each thread has:
   - Own stack (4KB)
   - Own registers (saved in TCB)
   ═══════════════════════════════════════════════════════════════ */

volatile int thread_a_count = 0;
volatile int thread_b_count = 0;
volatile int thread_c_count = 0;

void thread_a(void) {
    uart_puts("\n[Thread A] Started in shared process!\n");
    
    /* This thread outputs A's - all threads share heap/code/data */
    for (int iter = 0; iter < 3; iter++) {
        uart_puts("[A] work iteration ");
        uart_putdec(iter);
        uart_puts("\n");
    }
    
    uart_puts("[Thread A] Main loop\n");
    
    /* Run thread A - print 80 A's then yield */
    for (int i = 0; i < 80; i++) {
        uart_putc('A');
        thread_a_count++;
        if ((thread_a_count % 40) == 0) uart_putc('\n');
    }
}

void thread_b(void) {
    uart_puts("\n[Thread B] Started in shared process!\n");
    
    for (int iter = 0; iter < 3; iter++) {
        uart_puts("[B] work iteration ");
        uart_putdec(iter);
        uart_puts("\n");
    }
    
    uart_puts("[Thread B] Main loop\n");
    
    /* Run thread B - print 80 B's then yield */
    for (int i = 0; i < 80; i++) {
        uart_putc('B');
        thread_b_count++;
        if ((thread_b_count % 40) == 0) uart_putc('\n');
    }
}

void thread_c(void) {
    uart_puts("\n[Thread C] Started in shared process!\n");
    
    for (int iter = 0; iter < 3; iter++) {
        uart_puts("[C] work iteration ");
        uart_putdec(iter);
        uart_puts("\n");
    }
    
    uart_puts("[Thread C] Main loop\n");
    
    /* Run thread C - print 80 C's then yield */
    for (int i = 0; i < 80; i++) {
        uart_putc('C');
        thread_c_count++;
        if ((thread_c_count % 40) == 0) uart_putc('\n');
    }
}

/* ═══════════════════════════════════════════════════════════════
   KERNEL MAIN - Initialize and run threads
   ═══════════════════════════════════════════════════════════════ */

void kernel_main(void) {
    /* Initialize UART first */
    uart_init();

    uart_puts("\n");
    uart_puts("════════════════════════════════════════\n");
    uart_puts("   MY KERNEL - PHASE 4:                 \n");
    uart_puts("   THREADS IN ONE PROCESS                \n");
    uart_puts("════════════════════════════════════════\n\n");

    /* ── STEP 1: Memory management ── */
    uart_puts("=== MEMORY SETUP ===\n");
    uart_puts("RAM: ");
    uart_puthex(RAM_START);
    uart_puts(" - ");
    uart_puthex(RAM_END);
    uart_puts("\n");
    
    unsigned long free_start = PAGE_ALIGN((unsigned long)&__end);
    pmm_init(free_start, RAM_END);
    heap_init();
    uart_puts("\n");

    /* ── STEP 2: Setup Virtual Memory (MMU) ── */
    uart_puts("=== MMU & PAGE TABLES ===\n");
    mmu_init();
    uart_puts("\n");

    /* ── STEP 3: Initialize Thread Scheduler ── */
    uart_puts("=== THREAD SCHEDULER SETUP ===\n");
    thread_scheduler_init();
    uart_puts("\n");

    /* ── STEP 4: Setup Exception Handling and Timer ── */
    uart_puts("=== EXCEPTIONS & INTERRUPTS ===\n");
    exceptions_init();
    uart_puts("\n");

    /* ── STEP 5: Create multiple threads IN SINGLE PROCESS ── */
    uart_puts("=== CREATE THREADS (in one process) ===\n");
    
    int tid_a = create_thread(thread_a, "Thread_A");
    if (tid_a < 0) {
        uart_puts("ERROR: Failed to create Thread A!\n");
        while(1);
    }
    
    int tid_b = create_thread(thread_b, "Thread_B");
    if (tid_b < 0) {
        uart_puts("ERROR: Failed to create Thread B!\n");
        while(1);
    }
    
    int tid_c = create_thread(thread_c, "Thread_C");
    if (tid_c < 0) {
        uart_puts("ERROR: Failed to create Thread C!\n");
        while(1);
    }
    
    uart_puts("Success! 3 threads created in single process.\n");
    uart_puts("Scheduler: Round-Robin (50ms time slice)\n");
    uart_puts("Memory: All threads share code/data/heap!\n\n");

    /* ── STEP 6: Start Thread Execution Loop ── */
    uart_puts("════════════════════════════════════════\n");
    uart_puts("  THREADS RUNNING IN SHARED PROCESS     \n");
    uart_puts("════════════════════════════════════════\n\n");
    uart_puts("Watch A, B, C - they execute in parallel!\n");
    uart_puts("All threads share the same memory space.\n\n");
    
    uart_puts("Starting thread scheduler...\n\n");
    
    /* Simple round-robin: cycle through each thread */
    while(1) {
        uart_puts("\n--- Thread Cycle ---\n");
        
        /* Thread A's turn */
        thread_a();
        
        uart_puts("\n");
        
        /* Thread B's turn */
        thread_b();
        
        uart_puts("\n");
        
        /* Thread C's turn */
        thread_c();
        
        uart_puts("\n");
    }
    
    /* Never returns */
    while(1) { asm volatile("wfi"); }
}
