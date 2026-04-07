/*
 * usermode.c - User Mode Execution and Privilege Level Management
 *
 * Implements:
 * 1. User task creation and management
 * 2. Privilege level switching (EL1 ↔ EL0)
 * 3. Context restoration for user tasks
 */

#include "usermode.h"
#include "memory.h"
#include "uart.h"

/* User task pool - support up to 4 user tasks */
user_task_t user_tasks[4];  /* Made global so it can be accessed from kernel.c */
static user_task_t *current_user_task = NULL;
static u32 next_pid = 1;

/* Forward declarations */
extern void switch_to_user_mode_asm(user_task_t *task);

/*
 * Initialize user mode system
 * Called once during kernel startup
 */
void usermode_init(void) {
    uart_puts("=== USER MODE INITIALIZATION ===\n");
    
    /* Initialize all task slots to DEAD */
    for (int i = 0; i < 4; i++) {
        user_tasks[i].pid = 0;
        user_tasks[i].state = TASK_DEAD;
        user_tasks[i].name[0] = '\0';
    }
    
    current_user_task = NULL;
    uart_puts("User mode system initialized\n");
    uart_puts("Ready to create and run user tasks\n\n");
}

/*
 * Create a new user task
 * Allocates memory (code, stack, heap) and registers task
 * Returns task PID on success, -1 on failure
 */
i32 create_user_task(
    const char *name,
    void (*entry_point)(void),
    u64 stack_size,
    u64 heap_size
) {
    uart_puts("[USERMODE] Creating user task: ");
    uart_puts(name);
    uart_puts("\n");
    
    /* Find free task slot */
    user_task_t *task = NULL;
    for (int i = 0; i < 4; i++) {
        if (user_tasks[i].state == TASK_DEAD) {
            task = &user_tasks[i];
            break;
        }
    }
    
    if (!task) {
        uart_puts("ERROR: No free task slots!\n");
        return -1;
    }
    
    /* Allocate task ID and store name */
    task->pid = next_pid++;
    for (int i = 0; name[i] && i < 31; i++) {
        task->name[i] = name[i];
    }
    task->name[31] = '\0';
    
    /* Allocate code memory (must be page-aligned) */
    task->code_base = pmm_alloc_page();
    if (!task->code_base) {
        uart_puts("ERROR: Failed to allocate code page\n");
        return -1;
    }
    uart_puts("  Code page: 0x");
    uart_puthex(task->code_base);
    uart_puts("\n");
    
    /* Create a simple bootstrap stub in user memory
     * This loads the actual entry point and jumps to it
     * The bootstrap must run from user memory (EL0) addresses
     */
    u32 *code_ptr = (u32 *)task->code_base;
    
    /* Simple bootstrap: Load PC value and jump
     * This code will run in EL0 user mode
     * movz x0, #<entry_point_hi>  (load immediate into x0)
     * movk x0, #<entry_point_lo>
     * br x0                         (branch to x0)
     * BUT we also need to handle that entry_point is in kernel space
     * 
     * BETTER APPROACH: Just inline a simple SVC call as bootstrap
     * The actual user code will work via syscalls, not direct execution
     * For now, let's just create a minimal stub that does syscalls
     */
    
    /* Bootstrap code: simple infinite loop of syscalls
     * For testing: Just loop and let exception handlers take over
     * Later: We'll implement proper entry point handling
     * For now use NOP slide so first exception leads us to proper handling
     */
    
    /* CODE SECTION 1: Print task startup message via syscall */
    /* mov x0, #1          - SYS_PRINT syscall num */
    /* mov x1, #<msg_ptr>  - pointer to message */
    /* mov x2, #<msg_len>  - message length */
    /* svc #0              - syscall */
    /* b .                 - branch to self (loop forever) */
    
    /* Let's encode these as ARM64 instructions */
    /* For simplicity, just fill with svc #0 instructions that will trigger syscall handler */
    
    /* Fill user code page with executable user code
     * First instruction: SVC #0 - syscall to kernel
     * Remaining instructions: infinite loop (b . - branch to self)
     * 
     * This demonstrates:
     * 1. User code runs at EL0
     * 2. Executes SVC instruction
     * 3. Exception handler catches it and prints [SYSCALL]
     * 4. PC advances past SVC
     * 5. Infinite loop runs (visible as no more output)
     */
    code_ptr[0] = 0xD4000001;      /* svc #0 - syscall instruction */
    for (int i = 1; i < 1024; i++) {
        code_ptr[i] = 0x14000000;  /* b . - branch to self (infinite loop) */
    }
    
    /* PC points to user memory where our code is */
    task->pc = task->code_base;
    
    /* Allocate stack (user stack must be separate from kernel stack) */
    task->stack_base = pmm_alloc_page();
    if (!task->stack_base) {
        uart_puts("ERROR: Failed to allocate stack page\n");
        pmm_free_page(task->code_base);
        return -1;
    }
    task->stack_size = 4096;  /* 4KB page */
    task->sp = task->stack_base + task->stack_size - 16;  /* Stack grows down */
    uart_puts("  Stack page: 0x");
    uart_puthex(task->stack_base);
    uart_puts(" (SP=0x");
    uart_puthex(task->sp);
    uart_puts(")\n");
    
    /* Allocate heap (user heap for malloc) */
    task->heap_base = pmm_alloc_page();
    if (!task->heap_base) {
        uart_puts("ERROR: Failed to allocate heap page\n");
        pmm_free_page(task->code_base);
        pmm_free_page(task->stack_base);
        return -1;
    }
    task->heap_size = 4096;
    uart_puts("  Heap page: 0x");
    uart_puthex(task->heap_base);
    uart_puts("\n");
    
    /* Initialize user CPU registers to zero */
    for (int i = 0; i < 31; i++) {
        task->registers[i] = 0;
    }
    
    /* Set privilege level to EL0 (user mode) */
    task->exception_level = 0;
    
    /* Mark task as ready to run */
    task->state = TASK_READY;
    
    uart_puts("  Task PID ");
    uart_putdec(task->pid);
    uart_puts(" created successfully\n");
    
    return task->pid;
}

/*
 * Get the current running user task
 */
user_task_t* get_current_user_task(void) {
    return current_user_task;
}

/*
 * Switch to user mode for a specific task
 * This is called from kernel and should NOT return
 * Instead, it jumps to user code and runs it
 * When user code does a syscall, exception handler returns here
 */
void switch_to_user_mode(user_task_t *task) {
    if (!task) {
        uart_puts("ERROR: Invalid task pointer\n");
        return;
    }
    
    uart_puts("[KERNEL] Switching to user mode for task: ");
    uart_puts(task->name);
    uart_puts(" (PID ");
    uart_putdec(task->pid);
    uart_puts(")\n");
    uart_puts("  PC = 0x");
    uart_puthex(task->pc);
    uart_puts("\n");
    uart_puts("  SP = 0x");
    uart_puthex(task->sp);
    uart_puts("\n");
    
    /* Set current task */
    current_user_task = task;
    task->state = TASK_RUNNING;
    
    /* Call assembly function to perform actual privilege level switch */
    switch_to_user_mode_asm(task);
    
    /* Control should NOT reach here (unless user mode returns via exception) */
}

/*
 * Return to user mode after syscall handling
 * Called from exception handler to resume user code
 */
void return_to_user_mode(user_task_t *task) {
    if (!task) {
        uart_puts("ERROR: Invalid task for return_to_user\n");
        return;
    }
    
    uart_puts("[KERNEL] Returning to user mode: ");
    uart_puts(task->name);
    uart_puts("\n");
    
    /* Jump to switch_to_user_mode_asm which restores SP and jumps to PC */
    switch_to_user_mode_asm(task);
}

/*
 * Kill a user task and clean up resources
 */
void kill_user_task(user_task_t *task) {
    if (!task) {
        return;
    }
    
    uart_puts("[USERMODE] Killing task: ");
    uart_puts(task->name);
    uart_puts(" (PID ");
    uart_putdec(task->pid);
    uart_puts(")\n");
    
    /* Free allocated memory */
    if (task->code_base) {
        pmm_free_page(task->code_base);
    }
    if (task->stack_base) {
        pmm_free_page(task->stack_base);
    }
    if (task->heap_base) {
        pmm_free_page(task->heap_base);
    }
    
    /* Mark as dead */
    task->state = TASK_DEAD;
    task->pid = 0;
    
    /* Clear current task if it was the one being killed */
    if (current_user_task == task) {
        current_user_task = NULL;
    }
    
    uart_puts("  Task killed\n");
}

/*
 * ASSEMBLY CODE TO ACTUALLY SWITCH PRIVILEGE LEVELS
 * This MUST be in assembly because we need to:
 * 1. Set up ELR_EL1 (exception return address) with user code PC
 * 2. Set up SPSR_EL1 (saved status register) with EL0 bits set
 * 3. Set SP to user stack
 * 4. Execute ERET which returns to EL0
 */
asm(
    ".global switch_to_user_mode_asm\n"
    ".section .text.usermode_switch\n"
    "switch_to_user_mode_asm:\n"
    "    ldr x1, [x0, #48]\n"     /* Load PC from offset 48 (pc field) */
    "    ldr x2, [x0, #56]\n"     /* Load SP from offset 56 (sp field) */
    "    msr elr_el1, x1\n"       /* Set exception return address to user PC */
    "    mov x3, #0\n"            /* Set privilege level to 0 (EL0) */
    "    msr spsr_el1, x3\n"      /* Store in saved processor state */
    "    mov sp, x2\n"            /* Load user stack pointer */
    "    eret\n"                  /* PRIVILEGE CHANGE: EL1 → EL0 */
);

/*
 * Additional assembly for user mode helpers
 * These functions can be called from user mode code
 */
asm(
    ".global sys_print\n"
    ".section .text.syscalls\n"
    "sys_print:\n"
    "    mov x0, #1\n"
    "    svc #0\n"
    "    ret\n"
    "\n"
    ".global sys_exit\n"
    "sys_exit:\n"
    "    mov x0, #2\n"
    "    svc #0\n"
    "    ret\n"
    "\n"
    ".global sys_get_counter\n"
    "sys_get_counter:\n"
    "    mov x0, #3\n"
    "    svc #0\n"
    "    ret\n"
    "\n"
    ".global sys_sleep\n"
    "sys_sleep:\n"
    "    mov x1, x0\n"
    "    mov x0, #4\n"
    "    svc #0\n"
    "    ret\n"
);
