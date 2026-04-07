#ifndef THREAD_H
#define THREAD_H

#include "memory.h"

/* ═══════════════════════════════════════════════════════════════
   THREAD CONTROL BLOCK (TCB)
   
   Similar to PCB but for threads (lighter weight).
   Multiple threads share the SAME process memory space.
   Each thread has its own stack and registers.
   ═══════════════════════════════════════════════════════════════ */

/* Thread states */
#define THREAD_STATE_READY    1
#define THREAD_STATE_RUNNING  2
#define THREAD_STATE_BLOCKED  3
#define THREAD_STATE_DEAD     4

/* CPU Register Context (saved during thread switch) */
typedef struct {
    u64 x0, x1, x2, x3, x4, x5, x6, x7;
    u64 x8, x9, x10, x11, x12, x13, x14, x15;
    u64 x16, x17, x18, x19, x20, x21, x22, x23;
    u64 x24, x25, x26, x27, x28, x29, x30;
    u64 sp;  /* Stack pointer */
    u64 pc;  /* Program counter */
} tcb_context_t;

/* Thread Control Block - one per thread */
typedef struct {
    u32 tid;                        /* Thread ID (0-7) */
    u32 state;                      /* READY, RUNNING, BLOCKED, DEAD */
    tcb_context_t context;          /* Saved CPU register state */
    u64 stack_base;                 /* Bottom of thread's stack */
    u64 stack_size;                 /* 4KB per thread */
    u64 time_used;                  /* CPU time used (milliseconds) */
    char name[32];                  /* Thread name for debugging */
    void (*entry_point)(void);      /* Thread function pointer */
} tcb_t;

/* Thread scheduler state */
#define MAX_THREADS 8
#define THREAD_STACK_SIZE 4096
#define THREAD_TIME_SLICE_MS 50

typedef struct {
    tcb_t threads[MAX_THREADS];         /* Thread control blocks */
    u32 current_tid;                    /* Which thread is running */
    u32 num_threads;                    /* How many threads created */
    u64 context_switches;               /* Total context switches */
} thread_scheduler_t;

/* ═══════════════════════════════════════════════════════════════
   THREAD API
   ═══════════════════════════════════════════════════════════════ */

/* Initialize thread scheduler */
void thread_scheduler_init(void);

/* Create a new thread */
int create_thread(void (*entry_point)(void), const char* name);

/* Get current thread */
tcb_t* get_current_thread(void);

/* Get thread by ID */
tcb_t* get_thread_by_id(u32 tid);

/* Schedule next thread (called every timer interrupt) */
void schedule_next_thread(void);

/* Print scheduler statistics */
void thread_print_stats(void);

#endif
