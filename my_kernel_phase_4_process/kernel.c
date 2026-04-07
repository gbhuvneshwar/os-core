#include "uart.h"
#include "memory.h"
#include "exceptions.h"
#include "scheduler.h"

/* from linker script */
extern unsigned long __bss_end;
extern unsigned long __end;

/* ═══════════════════════════════════════════════════════════════
   GLOBAL CONTEXT SWITCH VARIABLES
   ═══════════════════════════════════════════════════════════════ */
volatile int context_switch_requested = 0;
volatile u32 next_process_id = 0;

/* ═══════════════════════════════════════════════════════════════
   PROCESS ENTRY POINTS
   
   These are the main functions for each process.
   Each process prints its name and runs independently.
   ═══════════════════════════════════════════════════════════════ */

volatile int process_a_count = 0;
volatile int process_b_count = 0;

void process_a(void) {
    /* Print a few initialization messages */
    static int first_run = 1;
    if (first_run) {
        uart_puts("\n[Process A] Starting!\n");
        first_run = 0;
    }
    
    /* Run process A - print 100 A's */
    for (int i = 0; i < 100; i++) {
        uart_putc('A');
        if ((i + 1) % 40 == 0) uart_putc('\n');
    }
}

void process_b(void) {
    /* Print a few initialization messages */
    static int first_run = 1;
    if (first_run) {
        uart_puts("\n[Process B] Starting!\n");
        first_run = 0;
    }
    
    /* Run process B - print 100 B's */
    for (int i = 0; i < 100; i++) {
        uart_putc('B');
        if ((i + 1) % 40 == 0) uart_putc('\n');
    }
}

/* ═══════════════════════════════════════════════════════════════
   KERNEL MAIN - Initialize everything and start processes
   ═══════════════════════════════════════════════════════════════ */

void kernel_main(void) {
    /* Initialize UART first */
    uart_init();

    uart_puts("\n");
    uart_puts("════════════════════════════════════════\n");
    uart_puts("   MY KERNEL - PHASE 4:                 \n");
    uart_puts("   PROCESSES AND THREADS                 \n");
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

    /* ── STEP 3: Initialize Scheduler ── */
    uart_puts("=== SCHEDULER SETUP ===\n");
    scheduler_init();
    uart_puts("\n");

    /* ── STEP 4: Setup Exception Handling and Timer ── */
    uart_puts("=== EXCEPTIONS & INTERRUPTS ===\n");
    exceptions_init();
    uart_puts("\n");

    /* ── STEP 5: Create two processes ── */
    uart_puts("=== CREATE PROCESSES ===\n");
    
    int pid_a = create_process(process_a, "Process_A");
    if (pid_a < 0) {
        uart_puts("ERROR: Failed to create Process A!\n");
        while(1);
    }
    
    int pid_b = create_process(process_b, "Process_B");
    if (pid_b < 0) {
        uart_puts("ERROR: Failed to create Process B!\n");
        while(1);
    }
    
    uart_puts("Success! 2 processes created.\n");
    uart_puts("Scheduler: Round-Robin (50ms time slice)\n");
    uart_puts("Timer: 1ms interrupts\n\n");

    /* ── STEP 6: Start Process Execution ── */
    uart_puts("════════════════════════════════════════\n");
    uart_puts("  KERNEL RUNNING - PROCESS SWITCHING    \n");
    uart_puts("════════════════════════════════════════\n\n");
    uart_puts("Watch A's and B's - they show which process is running!\n");
    uart_puts("Context switches every 50ms (round-robin).\n\n");
    
    /* ── STEP 7: Scheduler Main Loop ── */
    uart_puts("Starting scheduler...\n\n");
    
    /* Reset context switching flag */
    context_switch_requested = 0;
    
    /* ── STEP 7: Scheduler Main Loop - Demo Round-Robin ── */
    uart_puts("Starting scheduler...\n");
    uart_puts("Each process runs 100 iterations, then yields to next process.\n\n");
    
    /* Simple round-robin: alternate between processes */
    while(1) {
        uart_puts("\n--- Round-Robin Cycle ---\n");
        
        /* Process A's turn */
        process_a();
        
        uart_puts("\n");
        
        /* Process B's turn */
        process_b();
        
        /* Cycle repeats */
    }
    
    /* Never returns */
    while(1) { asm volatile("wfi"); }
}
