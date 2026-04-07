/*
 * usermode.h - User Mode Execution Support
 * 
 * Provides functions to:
 * 1. Create user mode tasks
 * 2. Switch from kernel (EL1) to user mode (EL0)
 * 3. Return from user mode syscalls back to kernel
 */

#ifndef USERMODE_H
#define USERMODE_H

#include "types.h"

/* User Task Control Block - tracks each user task */
typedef struct {
    u32 pid;                        /* Process ID */
    u32 state;                      /* 0=DEAD, 1=READY, 2=RUNNING, 3=BLOCKED */
    
    /* User mode memory layout */
    u64 code_base;                  /* Where user code starts in memory */
    u64 stack_base;                 /* Where user stack starts */
    u64 stack_size;                 /* Size of user stack */
    u64 heap_base;                  /* Where user heap starts */
    u64 heap_size;                  /* Size of user heap */
    
    /* CPU state for context switching */
    u64 pc;                         /* Program counter (user code entry) */
    u64 sp;                         /* Stack pointer */
    u64 registers[31];              /* x0-x30 */
    
    /* Privilege level info */
    u32 exception_level;            /* 0 = EL0, 1 = EL1 */
    
    char name[32];                  /* Task name for debugging */
} user_task_t;

/* User task states */
#define TASK_DEAD       0
#define TASK_READY      1
#define TASK_RUNNING    2
#define TASK_BLOCKED    3

/* Initialize user mode system */
void usermode_init(void);

/* Create a new user task */
i32 create_user_task(
    const char *name,
    void (*entry_point)(void),
    u64 stack_size,
    u64 heap_size
);

/* Get current user task */
user_task_t* get_current_user_task(void);

/* Switch to user mode (from EL1 to EL0) - never returns directly */
void switch_to_user_mode(user_task_t *task);

/* Called from exception handler to switch back to user */
void return_to_user_mode(user_task_t *task);

/* Kill a user task */
void kill_user_task(user_task_t *task);

/* Global user task pool - accessible from kernel */
extern user_task_t user_tasks[4];

#endif /* USERMODE_H */
