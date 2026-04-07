#include "scheduler.h"
#include "uart.h"
#include "memory.h"

/* ═══════════════════════════════════════════════════════════════
   SCHEDULER IMPLEMENTATION
   ═══════════════════════════════════════════════════════════════ */

/* Global scheduler state */
static scheduler_t scheduler = {
    .current_pid = 0,
    .process_count = 0,
    .context_switches = 0,
    .gil_lock = 0,
};

/* Time slice for each process (in 1ms timer ticks) */
#define TIME_SLICE_MS 50  /* 50ms per process */

/* ─────────────────────────────────────────────────────
   SCHEDULER INITIALIZATION
   ───────────────────────────────────────────────────── */

void scheduler_init(void) {
    uart_puts("=== SCHEDULER INIT ===\n");
    uart_puts("max processes: ");
    uart_putdec(MAX_PROCESSES);
    uart_puts("\n");
    uart_puts("time slice: ");
    uart_putdec(TIME_SLICE_MS);
    uart_puts("ms\n");
    uart_puts("GIL enabled (simple spinlock)\n\n");
    
    /* Initialize all PCBs */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        scheduler.processes[i].pid = i;
        scheduler.processes[i].state = PROC_STATE_DEAD;
        scheduler.processes[i].time_used = 0;
    }
    
    scheduler.current_pid = 0;
    scheduler.process_count = 0;
}

/* ─────────────────────────────────────────────────────
   PROCESS CREATION
   ───────────────────────────────────────────────────── */

int create_process(void (*entry_point)(void), const char* name) {
    /* Find a free process slot */
    int pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (scheduler.processes[i].state == PROC_STATE_DEAD) {
            pid = i;
            break;
        }
    }
    
    if (pid == -1) {
        uart_puts("create_process: no free slots!\n");
        return -1;
    }
    
    /* Get the PCB for this process */
    pcb_t* pcb = &scheduler.processes[pid];
    
    /* Allocate stack (one 4KB page) */
    u64 stack_phys = pmm_alloc_page();
    if (stack_phys == 0) {
        uart_puts("create_process: failed to allocate stack!\n");
        return -1;
    }
    
    /* Stack grows downward, so sp starts at top of stack */
    pcb->stack_base = stack_phys;
    pcb->stack_size = PAGE_SIZE;
    u64 stack_top = stack_phys + PAGE_SIZE;
    
    /* Copy process name */
    int i = 0;
    while (name && name[i] && i < 31) {
        pcb->name[i] = name[i];
        i++;
    }
    pcb->name[i] = '\0';
    
    /* Store entry point */
    pcb->entry_point = entry_point;
    
    /* Initialize CPU context */
    cpu_context_t* ctx = &pcb->context;
    
    /* Zero all registers */
    ctx->x0 = ctx->x1 = ctx->x2 = ctx->x3 = 0;
    ctx->x4 = ctx->x5 = ctx->x6 = ctx->x7 = 0;
    ctx->x8 = ctx->x9 = ctx->x10 = ctx->x11 = 0;
    ctx->x12 = ctx->x13 = ctx->x14 = ctx->x15 = 0;
    ctx->x16 = ctx->x17 = ctx->x18 = ctx->x19 = 0;
    ctx->x20 = ctx->x21 = ctx->x22 = ctx->x23 = 0;
    ctx->x24 = ctx->x25 = ctx->x26 = ctx->x27 = 0;
    ctx->x28 = ctx->x29 = 0;
    
    /* Set up stack pointer (stack grows downward) */
    ctx->sp = stack_top - 16;  /* Leave some room at top */
    
    /* Set up program counter to entry point */
    ctx->pc = (u64)entry_point;
    
    /* Set return register to spin infinitely
       If process does RET, it will loop forever instead of crashing
    */
    ctx->x30 = (u64)entry_point;  /* Return to self */
    
    /* Set process state to READY */
    pcb->state = PROC_STATE_READY;
    pcb->time_used = 0;
    pcb->created_at = 0;  /* Will be set to current tick later */
    
    scheduler.process_count++;
    
    uart_puts("created process: ");
    uart_puts(name);
    uart_puts(" (pid=");
    uart_putdec(pid);
    uart_puts(", stack=");
    uart_puthex(stack_phys);
    uart_puts(")\n");
    
    return pid;
}

/* ─────────────────────────────────────────────────────
   GET CURRENT PROCESS
   ───────────────────────────────────────────────────── */

pcb_t* get_current_process(void) {
    return &scheduler.processes[scheduler.current_pid];
}

u32 get_current_process_id(void) {
    return scheduler.current_pid;
}

pcb_t* get_process_by_id(u32 pid) {
    if (pid < MAX_PROCESSES) {
        return &scheduler.processes[pid];
    }
    return 0;
}

/* ─────────────────────────────────────────────────────
   ROUND-ROBIN SCHEDULER
   
   Every timer interrupt (1ms), we check if current process
   has used up its time slice. If yes, we switch to next process.
   ───────────────────────────────────────────────────── */

/* Global context switch request - defined in kernel.c */
extern volatile int context_switch_requested;
extern volatile u32 next_process_id;

void schedule_next_process(void) {
    /* Get current process */
    pcb_t* current = get_current_process();
    
    /* Increment its time used */
    current->time_used++;
    
    /* Check if it's time to switch (time slice expired) */
    if (current->time_used < TIME_SLICE_MS) {
        /* Still has time left, keep running */
        return;
    }
    
    /* Time slice expired, find next READY process */
    u32 next_pid = scheduler.current_pid;
    int checked = 0;
    
    while (checked < MAX_PROCESSES) {
        next_pid = (next_pid + 1) % MAX_PROCESSES;
        pcb_t* next = &scheduler.processes[next_pid];
        
        if (next->state == PROC_STATE_READY || 
            next->state == PROC_STATE_RUNNING) {
            break;
        }
        
        checked++;
    }
    
    /* If no other process found, loop back to current */
    if (next_pid == scheduler.current_pid) {
        /* No other process, reset time for current */
        current->time_used = 0;
        return;
    }
    
    /* Switch to next process */
    pcb_t* next = &scheduler.processes[next_pid];
    
    /* Mark context switch */
    scheduler.context_switches++;
    
    /* Print context switch info */
    if ((scheduler.context_switches % 50) == 0) {
        uart_puts("\n[");
        uart_putdec(scheduler.context_switches);
        uart_puts(" switches]");
    }
    
    /* Update process states */
    current->state = PROC_STATE_READY;
    next->state = PROC_STATE_RUNNING;
    next->time_used = 0;  /* Reset time slice for new process */
    
    /* Update current PID */
    scheduler.current_pid = next_pid;
    
    /* Set global flag indicating a context switch should happen */
    context_switch_requested = 1;
    next_process_id = next_pid;
}

/* ─────────────────────────────────────────────────────
   SCHEDULER STATISTICS
   ───────────────────────────────────────────────────── */

void scheduler_print_stats(void) {
    uart_puts("\n=== SCHEDULER STATISTICS ===\n");
    uart_puts("total context switches: ");
    uart_putdec(scheduler.context_switches);
    uart_puts("\n");
    uart_puts("processes:\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t* pcb = &scheduler.processes[i];
        if (pcb->state != PROC_STATE_DEAD) {
            uart_puts("  pid=");
            uart_putdec(pcb->pid);
            uart_puts(" name=");
            uart_puts(pcb->name);
            uart_puts(" state=");
            uart_putdec(pcb->state);
            uart_puts(" time_used=");
            uart_putdec(pcb->time_used);
            uart_puts("ms\n");
        }
    }
    
    uart_puts("============================\n\n");
}
