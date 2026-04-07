#include "thread.h"
#include "uart.h"

/* ═══════════════════════════════════════════════════════════════
   GLOBAL THREAD SCHEDULER
   
   Manages all threads in the single process.
   All threads share the same code/data/heap memory.
   Each thread has its own stack.
   ═══════════════════════════════════════════════════════════════ */

static thread_scheduler_t thread_scheduler = {0};

/* ═══════════════════════════════════════════════════════════════
   SCHEDULER INITIALIZATION
   ═══════════════════════════════════════════════════════════════ */

void thread_scheduler_init(void) {
    uart_puts("=== THREAD SCHEDULER INIT ===\n");
    uart_puts("max threads: ");
    uart_putdec(MAX_THREADS);
    uart_puts("\n");
    uart_puts("stack size: ");
    uart_putdec(THREAD_STACK_SIZE);
    uart_puts(" bytes per thread\n");
    uart_puts("time slice: ");
    uart_putdec(THREAD_TIME_SLICE_MS);
    uart_puts("ms\n");
    
    /* Initialize thread array */
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_scheduler.threads[i].tid = i;
        thread_scheduler.threads[i].state = THREAD_STATE_DEAD;
        thread_scheduler.threads[i].time_used = 0;
    }
    
    thread_scheduler.current_tid = 0;
    thread_scheduler.num_threads = 0;
    thread_scheduler.context_switches = 0;
    
    uart_puts("Thread scheduler ready!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
   CREATE NEW THREAD
   
   Allocates stack from PMM and initializes TCB.
   All threads share the same code/data/heap.
   ═══════════════════════════════════════════════════════════════ */

int create_thread(void (*entry_point)(void), const char* name) {
    /* Check if we have space */
    if (thread_scheduler.num_threads >= MAX_THREADS) {
        uart_puts("create_thread: max threads reached!\n");
        return -1;
    }
    
    /* Find first dead thread slot */
    u32 tid = 0;
    for (tid = 0; tid < MAX_THREADS; tid++) {
        if (thread_scheduler.threads[tid].state == THREAD_STATE_DEAD) {
            break;
        }
    }
    
    /* Allocate stack (4KB page from physical memory) */
    u64 stack_phys = pmm_alloc_page();
    if (stack_phys == 0) {
        uart_puts("create_thread: failed to allocate stack!\n");
        return -1;
    }
    
    /* Initialize TCB */
    tcb_t* tcb = &thread_scheduler.threads[tid];
    tcb->tid = tid;
    tcb->state = THREAD_STATE_READY;
    tcb->stack_base = stack_phys;
    tcb->stack_size = THREAD_STACK_SIZE;
    tcb->time_used = 0;
    tcb->entry_point = entry_point;
    
    /* Copy name */
    for (int i = 0; i < 31 && name[i]; i++) {
        tcb->name[i] = name[i];
    }
    tcb->name[31] = '\0';
    
    /* Initialize context */
    tcb->context.sp = stack_phys + THREAD_STACK_SIZE - 16;  /* Bottom of stack */
    tcb->context.pc = 0;  /* Will be set when thread runs */
    
    thread_scheduler.num_threads++;
    
    uart_puts("created thread: ");
    uart_puts(tcb->name);
    uart_puts(" (tid=");
    uart_putdec(tid);
    uart_puts(", stack=");
    uart_puthex(stack_phys);
    uart_puts(")\n");
    
    return tid;
}

/* ═══════════════════════════════════════════════════════════════
   GET CURRENT THREAD
   ═══════════════════════════════════════════════════════════════ */

tcb_t* get_current_thread(void) {
    return &thread_scheduler.threads[thread_scheduler.current_tid];
}

/* ═══════════════════════════════════════════════════════════════
   GET THREAD BY ID
   ═══════════════════════════════════════════════════════════════ */

tcb_t* get_thread_by_id(u32 tid) {
    if (tid < MAX_THREADS) {
        return &thread_scheduler.threads[tid];
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
   ROUND-ROBIN SCHEDULER
   
   Every timer interrupt (1ms), check if current thread's time
   slice has expired. If yes, switch to next READY thread.
   ═══════════════════════════════════════════════════════════════ */

void schedule_next_thread(void) {
    /* Get current thread */
    tcb_t* current = get_current_thread();
    
    /* Increment its time used */
    current->time_used++;
    
    /* Check if it's time to switch (time slice expired) */
    if (current->time_used < THREAD_TIME_SLICE_MS) {
        /* Still has time left, keep running */
        return;
    }
    
    /* Time slice expired, find next READY thread */
    u32 next_tid = thread_scheduler.current_tid;
    int checked = 0;
    
    while (checked < MAX_THREADS) {
        next_tid = (next_tid + 1) % MAX_THREADS;
        tcb_t* next = &thread_scheduler.threads[next_tid];
        
        if (next->state == THREAD_STATE_READY || 
            next->state == THREAD_STATE_RUNNING) {
            break;
        }
        
        checked++;
    }
    
    /* If no other thread found, reset current thread's time */
    if (next_tid == thread_scheduler.current_tid) {
        current->time_used = 0;
        return;
    }
    
    /* Switch to next thread */
    tcb_t* next = &thread_scheduler.threads[next_tid];
    
    /* Mark context switch */
    thread_scheduler.context_switches++;
    
    /* Print context switch info every 20 switches */
    if ((thread_scheduler.context_switches % 20) == 0) {
        uart_puts("\n[switch ");
        uart_putdec(thread_scheduler.context_switches);
        uart_puts(": ");
        uart_puts(current->name);
        uart_puts(" -> ");
        uart_puts(next->name);
        uart_puts("]");
    }
    
    /* Update thread states */
    current->state = THREAD_STATE_READY;
    next->state = THREAD_STATE_RUNNING;
    next->time_used = 0;  /* Reset time slice for new thread */
    
    /* Update current TID */
    thread_scheduler.current_tid = next_tid;
}

/* ═══════════════════════════════════════════════════════════════
   SCHEDULER STATISTICS
   ═══════════════════════════════════════════════════════════════ */

void thread_print_stats(void) {
    uart_puts("\n=== THREAD SCHEDULER STATISTICS ===\n");
    uart_puts("total context switches: ");
    uart_putdec(thread_scheduler.context_switches);
    uart_puts("\n");
    uart_puts("threads created: ");
    uart_putdec(thread_scheduler.num_threads);
    uart_puts("\n");
    uart_puts("current thread: ");
    uart_puts(get_current_thread()->name);
    uart_puts("\n");
}
