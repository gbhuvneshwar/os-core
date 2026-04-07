/*
 * kernel.c - PHASE 5: USER MODE AND PRIVILEGE LEVELS
 *
 * This kernel demonstrates:
 * 1. Running code in two privilege levels (EL1=kernel, EL0=user)
 * 2. System calls (SVC) for user->kernel communication
 * 3. Privilege level switching via ERET instruction
 * 4. Exception handlers for SVC exceptions
 *
 * Key differences from Phase 4:
 * - Phase 4: Everything runs in EL1 (kernel mode)
 * - Phase 5: User code runs in EL0, kernel in EL1
 * - Phase 5: SVC exceptions allow user->kernel transitions
 */

#include "types.h"
#include "uart.h"
#include "memory.h"
#include "exceptions.h"
#include "usermode.h"

/* Forward declarations for user tasks */
extern void user_task_a(void);
extern void user_task_b(void);
extern void user_task_c(void);
extern void simple_user_task(void);
extern void simple_user_mode_code(void);

/* ═══════════════════════════════════════════════════════════════
   KERNEL MAIN - PHASE 5 USER MODE SETUP
   ═══════════════════════════════════════════════════════════════ */

void kernel_main(void) {
    /* ─────────────────────────────────────────────────────
       SYSTEM INITIALIZATION (runs in EL1 kernel mode)
       ───────────────────────────────────────────────────── */
    
    uart_puts("\n");
    uart_puts("╔═════════════════════════════════════════════════════════════╗\n");
    uart_puts("║                                                             ║\n");
    uart_puts("║  PHASE 5: USER MODE AND PRIVILEGE LEVELS                   ║\n");
    uart_puts("║                                                             ║\n");
    uart_puts("║  What's different from Phase 4:                            ║\n");
    uart_puts("║  - Phase 4: All code in EL1 (kernel mode)                  ║\n");
    uart_puts("║  - Phase 5: User code in EL0, kernel in EL1                ║\n");
    uart_puts("║  - System calls (SVC) for secure transitions               ║\n");
    uart_puts("║    between privilege levels                                ║\n");
    uart_puts("║                                                             ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════════╝\n\n");
    
    /* Step 1: Initialize physical memory management */
    pmm_init(0x40100000, 0x48000000);  /* Initialize PMM with available RAM */
    uart_puts("Memory management initialized\n\n");
    
    /* Step 2: Initialize exceptions and interrupts */
    exceptions_init();
    uart_puts("\n");
    
    /* Step 3: Initialize user mode system */
    usermode_init();
    
    /* ─────────────────────────────────────────────────────
       CREATE AND RUN USER TASKS
       ───────────────────────────────────────────────────── */
    
    uart_puts("\n");
    uart_puts("╔═════════════════════════════════════════════════════════════╗\n");
    uart_puts("║  CREATING USER TASKS                                        ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════════╝\n\n");
    
    /* Create first user task */
    uart_puts("Creating Task A...\n");
    i32 pid_a = create_user_task(
        "Task_A",
        (void (*)(void))user_task_a,
        4096,      /* stack size: 4KB */
        4096       /* heap size: 4KB */
    );
    
    if (pid_a < 0) {
        uart_puts("ERROR: Failed to create Task A\n");
        while (1) {}
    }
    
    uart_puts("\n");
    
    /* Create second user task */
    uart_puts("Creating Task B...\n");
    i32 pid_b = create_user_task(
        "Task_B",
        (void (*)(void))user_task_b,
        4096,
        4096
    );
    
    if (pid_b < 0) {
        uart_puts("ERROR: Failed to create Task B\n");
        while (1) {}
    }
    
    uart_puts("\n");
    
    /* Create third user task */
    uart_puts("Creating Task C...\n");
    i32 pid_c = create_user_task(
        "Task_C",
        (void (*)(void))user_task_c,
        4096,
        4096
    );
    
    if (pid_c < 0) {
        uart_puts("ERROR: Failed to create Task C\n");
        while (1) {}
    }
    
    uart_puts("\n");
    
    /* ─────────────────────────────────────────────────────
       PRIVILEGE LEVEL TRANSITION TO USER MODE
       ───────────────────────────────────────────────────── */
    
    uart_puts("╔═════════════════════════════════════════════════════════════╗\n");
    uart_puts("║  SWITCHING FROM KERNEL MODE (EL1) TO USER MODE (EL0)       ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════════╝\n\n");
    
    uart_puts("This is the critical moment!\n");
    uart_puts("After this, we run in EL0 and can only:\n");
    uart_puts("  ✓ Use syscalls to request kernel services\n");
    uart_puts("  ✓ Access our own memory (code/stack/heap)\n");
    uart_puts("  ✗ Access hardware directly\n");
    uart_puts("  ✗ Modify privilege levels\n");
    uart_puts("  ✗ Modify page tables\n\n");
    
    uart_puts("Executing ERET to switch to EL0...\n\n");
    
    /* Get first user task and switch to it */
    user_task_t *task_a = get_current_user_task();
    
    /* Find Task A in task list  */
    for (int i = 0; i < 4; i++) {
        if (user_tasks[i].pid == pid_a) {
            task_a = &user_tasks[i];
            break;
        }
    }
    
    if (task_a && (u32)task_a->pid == (u32)pid_a) {
        /* PC is already set by create_user_task to point to user memory (0x40100000)
         * DO NOT override it with kernel code address!
         * The user memory contains SVC #0 instructions that will be executed in EL0
         */
        
        /* This switches CPU from EL1 to EL0 and runs user code */
        switch_to_user_mode(task_a);
        
        /* Control should NEVER reach here! */
        uart_puts("ERROR: Control returned from switch_to_user_mode!\n");
        while (1) {}
    }
    
    uart_puts("ERROR: Could not find Task A\n");
    while (1) {}
}

/* Simple user mode code - assembly-only, no C functions */
asm(
    ".global simple_user_mode_code\n"
    ".section .text.user_code\n"
    "simple_user_mode_code:\n"
    "    adr x1, user_msg\n"
    "    mov x0, #1\n"          /* SYS_PRINT */
    "    mov x2, #34\n"         /* Message length */
    "    svc #0\n"              /* Syscall! */
    "\n"
    "    adr x1, success_msg\n"
    "    mov x0, #1\n"          /* SYS_PRINT */
    "    mov x2, #39\n"          /* Message length */
    "    svc #0\n"              /* Syscall! */
    "\n"
    "    mov x0, #2\n"          /* SYS_EXIT */
    "    mov x1, #0\n"          /* status = 0 */
    "    svc #0\n"              /* Exit syscall */
    "\n"
    "    b .\n"                 /* Infinite loop */
    "\n"
    ".section .rodata.user\n"
    "user_msg:\n"
    "    .asciz \"USER MODE: This is EL0 code!\\n\"\n"
    "success_msg:\n"
    "    .asciz \"SUCCESS: Syscall from user!\\n\"\n"
);

/* ═══════════════════════════════════════════════════════════════
   PRIVILEGE LEVEL EXPLANATION
   ═══════════════════════════════════════════════════════════════

   ARM64 has 4 privilege levels (Exception Levels):

   EL3 (Secure Monitor)
   ├─ Highest privilege
   ├─ Firmware, secure code
   └─ Not used in our QEMU setup

   EL2 (Hypervisor)
   ├─ Virtual machine support
   ├─ Between kernel and secure
   └─ Not used in our QEMU setup

   EL1 (Kernel Mode) ← We are HERE when kernel code runs
   ├─ Operating system kernel
   ├─ Device drivers
   ├─ Full hardware access
   ├─ Can modify page tables
   ├─ Can change privilege levels
   └─ This is where we boot up

   EL0 (User Mode) ← After ERET, user code runs HERE
   ├─ User applications
   ├─ Restricted access
   ├─ Cannot touch hardware directly
   ├─ Must use syscalls for kernel services
   └─ Isolated from other processes

   TRANSITIONS:

   EL1 → EL0: Use ERET instruction with SPSR_EL1[3:0] = 0000
   EL0 → EL1: Use SVC instruction (causes exception)

   ═══════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════
   HOW SYSCALLS WORK
   ═══════════════════════════════════════════════════════════════

   USER MODE CODE:
   ┌─────────────────────────┐
   │ mov x0, #1              │ ← Set syscall number (SYS_PRINT)
   │ adr x1, message         │ ← Set first argument
   │ mov x2, #13             │ ← Set second argument  
   │ svc #0                  │ ← SYSCALL! Jump to kernel
   │ <--- exception happens here
   │ (return here when done)
   │ ...next instruction...  │
   └─────────────────────────┘
                ↓
        Exception occurs!
        CPU recognizes SVC
        Saves user registers
        Jumps to exception handler
                ↓
   KERNEL EXCEPTION HANDLER (EL1):
   ┌─────────────────────────────────────────┐
   │ exc_svc_handler() is called             │
   │ ├─ Check ESR_EL1 = SVC exception        │
   │ ├─ x0 = 1 (syscall number)              │
   │ ├─ x1 = message pointer                 │
   │ ├─ x2 = 13 (message length)             │
   │ ├─ dispatch via handle_syscall()        │
   │ │                                       │
   │ └─ handle_syscall(1) case SYS_PRINT:   │
   │    uart_puts((char*)x1);                │
   │    x0 = 0; (return success)             │
   │                                         │
   │ eret  ← Return to user mode             │
   └─────────────────────────────────────────┘
                ↓
        Returns to user code!
        x0 = 0 (return value)
        User continues...

   ═══════════════════════════════════════════════════════════════ */
