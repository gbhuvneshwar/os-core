#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "memory.h"

/* ═══════════════════════════════════════════════════════════════
   PROCESS CONTROL BLOCK (PCB)
   
   This structure holds all information about a single process
   including its CPU register state, memory usage, and scheduling info.
   ═══════════════════════════════════════════════════════════════ */

/*
   CPU Register State
   ARM64 has 31 general-purpose registers (x0-x30)
   x29 = frame pointer
   x30 = link register (return address)
   sp  = stack pointer
   pc  = program counter
   
   We save ALL registers during context switch so we can
   restore them exactly as they were.
*/
typedef struct {
    u64 x0, x1, x2, x3, x4, x5, x6, x7;
    u64 x8, x9, x10, x11, x12, x13, x14, x15;
    u64 x16, x17, x18, x19, x20, x21, x22, x23;
    u64 x24, x25, x26, x27, x28, x29, x30;
    u64 sp;
    u64 pc;
} cpu_context_t;

/* Process states */
#define PROC_STATE_READY    1
#define PROC_STATE_RUNNING  2
#define PROC_STATE_BLOCKED  3
#define PROC_STATE_DEAD     4

/* Process Control Block */
typedef struct {
    u32 pid;                    /* Process ID */
    u32 state;                  /* PROC_STATE_* */
    cpu_context_t context;      /* Saved CPU state */
    u64 stack_base;             /* Bottom of stack */
    u64 stack_size;             /* Stack size (4KB per process) */
    u64 time_used;              /* Total CPU time used (in ticks) */
    u64 created_at;             /* Tick when process was created */
    char name[32];              /* Process name for debugging */
    void (*entry_point)(void);  /* Entry point function */
} pcb_t;

/* ═══════════════════════════════════════════════════════════════
   SCHEDULER STATE
   ═══════════════════════════════════════════════════════════════ */

#define MAX_PROCESSES 4

/* Global scheduler state */
typedef struct {
    pcb_t processes[MAX_PROCESSES];     /* All processes */
    u32 current_pid;                    /* Currently running process */
    u32 process_count;                  /* Number of processes */
    u64 context_switches;               /* Total context switches */
    volatile int gil_lock;              /* Global Interpreter Lock (GIL) */
} scheduler_t;

/* ═══════════════════════════════════════════════════════════════
   SCHEDULER API
   ═══════════════════════════════════════════════════════════════ */

/* Initialize scheduler */
void scheduler_init(void);

/* Create a new process
   entry_point  = function address to run
   name         = process name (for debugging)
   return       = new process PID (-1 if failed)
*/
int create_process(void (*entry_point)(void), const char* name);

/* Get current process */
pcb_t* get_current_process(void);

/* Get current process ID */
u32 get_current_process_id(void);

/* Get process by ID */
pcb_t* get_process_by_id(u32 pid);

/* Schedule next process (called by timer interrupt)
   This is the main scheduling function - round-robin!
*/
void schedule_next_process(void);

/* Perform context switch (would be assembly in a full implementation)
   For Phase 4 demo, we show scheduling logic without actual register swapping.
   In a full OS, this would be:
   - Save current process's registers
   - Load next process's registers
   - Jump to next process's instruction pointer
*/
/* extern void context_switch(cpu_context_t* old_context, 
                           cpu_context_t* new_context); */

/* Print scheduler statistics */
void scheduler_print_stats(void);

/* ═══════════════════════════════════════════════════════════════
   GIL (Global Interpreter Lock)
   
   Prevents two processes from accessing kernel data simultaneously.
   This is a VERY simple spinlock implementation.
   
   In real OSes, you'd use mutexes, semaphores, or spinlocks.
   We use atomic operations with a simple flag here.
   ═══════════════════════════════════════════════════════════════ */

static inline void gil_acquire(void) {
    /* Busy-wait until we can acquire the lock */
    /* For now, we skip GIL since we're single-proccess per CPU core */
    /* In a real implementation, you'd use atomic cmpxchg here */
}

static inline void gil_release(void) {
    /* Release the lock */
    /* In a real implementation, you'd use atomic operations */
}

#endif
