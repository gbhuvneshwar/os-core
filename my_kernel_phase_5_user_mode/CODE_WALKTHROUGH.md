# Phase 5: Source Code Walkthrough

## Complete Code Explanation - Line by Line

---

## File 1: kernel.c - Initialization

### Complete Listing with Annotations

```c
/*
 * kernel.c - Phase 5 Kernel Main Entry Point
 * 
 * This is the first C code that runs after boot.S
 * It sets up the entire Phase 5 system:
 * 1. Physical Memory Manager
 * 2. Exception handling
 * 3. User mode system
 * 4. Creates user tasks
 * 5. Switches to user mode
 */

#include "uart.h"
#include "memory.h"
#include "exceptions.h"
#include "usermode.h"

/*
 * KERNEL MAIN - The heart of Phase 5
 * Called from boot.S after all bare-metal setup is done
 */
void kernel_main(void) {
    // ═══════════════════════════════════════════════════════════
    // STEP 1: PRINT HEADER
    // ═══════════════════════════════════════════════════════════
    // Output: Visual separator for clarity when running in QEMU
    uart_puts("╔═════════════════════════════════════════════════════════╗\n");
    uart_puts("║  PHASE 5: USER MODE AND PRIVILEGE LEVELS              ║\n");
    uart_puts("║  What's different from Phase 4:                       ║\n");
    uart_puts("║  - Phase 4: All code in EL1 (kernel mode)             ║\n");
    uart_puts("║  - Phase 5: User code in EL0, kernel in EL1           ║\n");
    uart_puts("║  - System calls (SVC) for secure transitions          ║\n");
    uart_puts("║    between privilege levels                           ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════╝\n");
    uart_puts("\n");

    // ═══════════════════════════════════════════════════════════
    // STEP 2: INITIALIZE PHYSICAL MEMORY MANAGER (PMM)
    // ═══════════════════════════════════════════════════════════
    // This discovers how much RAM is available and sets up
    // the free page list. Without this, we cannot allocate memory
    // for user tasks!
    pmm_init();
    uart_puts("Memory management initialized\n\n");

    // ═══════════════════════════════════════════════════════════
    // STEP 3: INSTALL EXCEPTION VECTOR TABLE
    // ═══════════════════════════════════════════════════════════
    // Tell the CPU where our exception handlers live.
    // This is critical for syscalls to work!
    // 
    // When a user program executes SVC #0:
    // 1. CPU raises exception
    // 2. CPU reads VBAR_EL1 (exception vector base address)
    // 3. CPU finds the SVC handler at VBAR_EL1 + 0x400
    // 4. CPU jumps to our exc_svc_handler function
    exceptions_init();
    // Output: "=== EXCEPTIONS & INTERRUPTS ==="
    //         "Installing exception vector table..."
    //         "Exception vector table at: 0x40081000"

    // ═══════════════════════════════════════════════════════════
    // STEP 4: INITIALIZE USER MODE SYSTEM
    // ═══════════════════════════════════════════════════════════
    // Set up the global user task pool
    // This must be done before creating any user tasks
    usermode_init();
    // Output: "=== USER MODE INITIALIZATION ==="
    //         "User mode system initialized"
    //         "Ready to create and run user tasks"

    // ═══════════════════════════════════════════════════════════
    // STEP 5: CREATE USER TASKS
    // ═══════════════════════════════════════════════════════════
    // Create 3 user tasks. Each gets:
    // - 4KB code page (filled with SVC instruction + infinite loop)
    // - 4KB stack page (for local variables, return addresses)
    // - 4KB heap page (unused for now, but allocated for future malloc)
    
    // WHY 4KB? Because that's the page size in Phase 6 (virtual memory)
    uranium_puts("╔═════════════════════════════════════════════════════════╗\n");
    uart_puts("║  CREATING USER TASKS                                    ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════╝\n");
    uart_puts("\n");

    uart_puts("Creating Task A...\n");
    i32 pid_a = create_user_task(
        "Task_A",                  // Task name (for debugging)
        NULL,                      // Entry point (unused - we use SVC loop)
        4096,                      // Stack size (4KB)
        4096                       // Heap size (4KB)
    );
    // Output: "[USERMODE] Creating user task: Task_A"
    //         "  Code page: 0x0x40100000"
    //         "  Stack page: 0x0x40101000 (SP=0x0x40101FF0)"
    //         "  Heap page: 0x0x40102000"
    //         "  Task PID 1 created successfully"
    
    uart_puts("Creating Task B...\n");
    i32 pid_b = create_user_task("Task_B", NULL, 4096, 4096);
    // Output: Similar to Task A, but with addresses:
    //         Code: 0x40103000, Stack: 0x40104000, Heap: 0x40105000
    
    uart_puts("Creating Task C...\n");
    i32 pid_c = create_user_task("Task_C", NULL, 4096, 4096);
    // Output: Similar to Task B, but with addresses:
    //         Code: 0x40106000, Stack: 0x40107000, Heap: 0x40108000

    // ═══════════════════════════════════════════════════════════
    // STEP 6: THIS IS THE CRITICAL MOMENT - SWITCH TO USER MODE
    // ═══════════════════════════════════════════════════════════
    // We're currently in EL1 (kernel mode), executing kernel code.
    // After this line, we switch to EL0 (user mode) and never return
    // to this function!
    
    uart_puts("\n");
    uart_puts("╔═════════════════════════════════════════════════════════╗\n");
    uart_puts("║  SWITCHING FROM KERNEL MODE (EL1) TO USER MODE (EL0)   ║\n");
    uart_puts("╚═════════════════════════════════════════════════════════╝\n");
    uart_puts("\n");
    
    uart_puts("This is the critical moment!\n");
    uart_puts("After this, we run in EL0 and can only:\n");
    uart_puts("  ✓ Use syscalls to request kernel services\n");
    uart_puts("  ✓ Access our own memory (code/stack/heap)\n");
    uart_puts("  ✗ Access hardware directly\n");
    uart_puts("  ✗ Modify privilege levels\n");
    uart_puts("  ✗ Modify page tables\n");
    uart_puts("\n");
    
    uart_puts("Executing ERET to switch to EL0...\n");
    uart_puts("\n");
    
    // Look up task A by PID to get its pointer
    user_task_t *task_a = NULL;
    for (int i = 0; i < 4; i++) {
        if (user_tasks[i].pid == pid_a) {
            task_a = &user_tasks[i];
            break;
        }
    }
    
    // Switch to user mode for Task A
    // IMPORTANT: This function NEVER RETURNS!
    // The next line executed will be in EL0 (user mode) at
    // address 0x40100000, which contains our SVC instruction!
    if (task_a) {
        switch_to_user_mode(task_a);
    }
    
    // THIS LINE WILL NEVER BE REACHED!
    // (Unless we implement exception-based task switching later)
}
```

---

## File 2: usermode.c - User Mode Management

### Key Function: switch_to_user_mode_asm (The Magic)

```asm
/*
 * ASSEMBLY CODE - This switches CPU privilege level
 * ARM64 assembly - runs at extremely low level
 */

switch_to_user_mode_asm:
    # x0 = pointer to user_task_t structure (passed by caller)
    # 
    # user_task_t structure layout in memory:
    #   +0:  u32 pid                (4 bytes)
    #   +4:  u32 state              (4 bytes)
    #   +8:  u64 code_base          (8 bytes)
    #   +16: u64 stack_base         (8 bytes)
    #   +24: u64 stack_size         (8 bytes)
    #   +32: u64 heap_base          (8 bytes)
    #   +40: u64 heap_size          (8 bytes)
    #   +48: u64 pc ← WE READ THIS
    #   +56: u64 sp ← WE READ THIS
    #   +64: u64 registers[31]      (31 * 8 bytes)
    #   ...
    
    # STEP 1: Load user PC from task structure
    ldr x1, [x0, #48]
    # x1 = user_task->pc = 0x40100000
    # x0 is base address of task structure
    # [x0, #48] means: read 8 bytes at (address in x0) + 48
    #
    # Why offset #48?
    #   4 (pid) + 4 (state) + 8 (code_base) + 8 (stack_base) +
    #   8 (stack_size) + 8 (heap_base) + 8 (heap_size) = 48
    
    # STEP 2: Load user SP from task structure
    ldr x2, [x0, #56]
    # x2 = user_task->sp = 0x40101FF0
    # Offset +56 = +48 (pc field) + 8 bytes
    
    # STEP 3: SET UP EXCEPTION LINK REGISTER
    # This tells CPU where to jump AFTER ERET
    msr elr_el1, x1
    # elr_el1 = x1 = 0x40100000 (user code starts here)
    # MSR = "Move to System Register"
    # When ERET executes, CPU reads ELR_EL1 and sets PC = ELR_EL1
    
    # STEP 4: SET UP SAVED PROCESSOR STATE
    # This tells CPU what privilege level to use after ERET
    mov x3, #0
    # x3 = 0
    # Binary: 0b0000
    # Bits [3:0] = 0000 = EL0 (user mode)
    
    msr spsr_el1, x3
    # spsr_el1 = x3 = 0
    # SPSR = "Saved Processor State Register"
    # When ERET executes:
    #   CPU reads SPSR_EL1[3:0]
    #   If 0b00: Privilege level becomes EL0 (USER MODE!)
    #   If 0b01: Privilege level becomes EL1 (kernel mode)
    
    # STEP 5: LOAD USER STACK POINTER
    mov sp, x2
    # sp = x2 = 0x40101FF0
    # SP = "Stack Pointer"
    # This is the kernel stack pointer register
    # After we change it to user stack, kernel stack is no longer accessible
    # Now we're using user stack!
    
    # STEP 6: MAGIC HAPPENS HERE - EXECUTE ERET
    eret
    # ERET = "Exception Return"
    # This instruction does the privilege level switch!
    #
    # What happens:
    # 1. CPU reads ELR_EL1 = 0x40100000
    # 2. CPU reads SPSR_EL1[3:0] = 0 (EL0)
    # 3. Current privilege level changes: EL1 → EL0
    # 4. PC = ELR_EL1 = 0x40100000
    # 5. Execution continues at 0x40100000 (in user mode!)
    # 6. Next instruction: 0xD4000001 (SVC #0)
    # 7. Exception is raised immediately!
    #
    # Result: We're now executing user code in EL0 privilege level!
```

### Key Function: create_user_task (Allocate Memory)

```c
i32 create_user_task(
    const char *name,
    void (*entry_point)(void),
    u64 stack_size,
    u64 heap_size
) {
    uart_puts("[USERMODE] Creating user task: ");
    uart_puts(name);
    uart_puts("\n");
    
    // ─────────────────────────────────────────────────────────
    // STEP 1: Find free task slot in global user_task_t array
    // ─────────────────────────────────────────────────────────
    user_task_t *task = NULL;
    for (int i = 0; i < 4; i++) {
        if (user_tasks[i].state == TASK_DEAD) {
            // Found a dead task slot, use it
            task = &user_tasks[i];
            break;
        }
    }
    
    if (!task) {
        uart_puts("ERROR: No free task slots!\n");
        return -1;  // Failure
    }
    
    // ─────────────────────────────────────────────────────────
    // STEP 2: Initialize task metadata
    // ─────────────────────────────────────────────────────────
    task->pid = next_pid++;  // Assign unique process ID (1, 2, 3, ...)
    
    // Copy task name to task structure
    for (int i = 0; name[i] && i < 31; i++) {
        task->name[i] = name[i];
    }
    task->name[31] = '\0';  // Null terminate
    
    // ─────────────────────────────────────────────────────────
    // STEP 3: ALLOCATE CODE PAGE (4KB)
    // ─────────────────────────────────────────────────────────
    task->code_base = pmm_alloc_page();
    // pmm_alloc_page() returns:
    //   First call: 0x40100000 (page 1)
    //   Second call: 0x40103000 (page 4)
    //   Third call: 0x40106000 (page 7)
    // (Each page is 4KB = 0x1000 bytes)
    
    if (!task->code_base) {
        uart_puts("ERROR: Failed to allocate code page\n");
        return -1;
    }
    
    uart_puts("  Code page: 0x");
    uart_puthex(task->code_base);
    uart_puts("\n");
    
    // ─────────────────────────────────────────────────────────
    // STEP 4: FILL CODE PAGE WITH USER CODE
    // ─────────────────────────────────────────────────────────
    // The "code page" is just raw memory we got from PMM
    // We now fill it with machine code (bytecode)
    
    u32 *code_ptr = (u32 *)task->code_base;
    // Create a pointer to the code page
    // Cast as u32* because ARM64 instructions are 32-bit
    
    // First instruction: SVC #0 (System Call)
    code_ptr[0] = 0xD4000001;
    //             ^^^^^^^^
    //             ARM64 machine code for SVC #0
    // When user code executes this, it triggers an exception
    // that brings us to exc_svc_handler in kernel mode
    
    // Remaining instructions: Infinite loop
    for (int i = 1; i < 1024; i++) {
        // 1024 words * 4 bytes = 4096 bytes = 1 page
        code_ptr[i] = 0x14000000;
        //             ^^^^^^^^
        //             ARM64 machine code for: b . (branch to self)
        // This creates an infinite loop of branching to itself
        // If user code ever returns from a syscall, it loops here
    }
    
    task->pc = task->code_base;
    // Program counter starts at beginning of code page (0x40100000)
    
    // ─────────────────────────────────────────────────────────
    // STEP 5: ALLOCATE STACK PAGE (4KB)
    // ─────────────────────────────────────────────────────────
    task->stack_base = pmm_alloc_page();
    // pmm_alloc_page() returns:
    //   Task A: 0x40101000
    //   Task B: 0x40104000
    //   Task C: 0x40107000
    
    if (!task->stack_base) {
        uart_puts("ERROR: Failed to allocate stack page\n");
        pmm_free_page(task->code_base);  // Free code page on failure
        return -1;
    }
    
    task->stack_size = 4096;  // Stack is 1 page = 4KB
    
    // Stack grows DOWNWARD (from high to low addresses)
    // So SP points to TOP of stack minus 16 bytes (safety margin)
    task->sp = task->stack_base + task->stack_size - 16;
    // Task A: 0x40101000 + 4096 - 16 = 0x40101FF0
    // This is where user code's stack pointer will start
    
    uart_puts("  Stack page: 0x");
    uart_puthex(task->stack_base);
    uart_puts(" (SP=0x");
    uart_puthex(task->sp);
    uart_puts(")\n");
    
    // ─────────────────────────────────────────────────────────
    // STEP 6: ALLOCATE HEAP PAGE (4KB)
    // ─────────────────────────────────────────────────────────
    // Heap is where malloc() allocations go (for Phase 6+)
    task->heap_base = pmm_alloc_page();
    
    if (!task->heap_base) {
        uart_puts("ERROR: Failed to allocate heap page\n");
        pmm_free_page(task->code_base);    // Free code page
        pmm_free_page(task->stack_base);   // Free stack page
        return -1;
    }
    
    task->heap_size = 4096;    // Heap is 1 page = 4KB
    
    uart_puts("  Heap page: 0x");
    uart_puthex(task->heap_base);
    uart_puts("\n");
    
    // ─────────────────────────────────────────────────────────
    // STEP 7: MARK TASK AS READY
    // ─────────────────────────────────────────────────────────
    task->state = TASK_RUNNING;
    // (or TASK_READY if implementing scheduler)
    
    // ─────────────────────────────────────────────────────────
    // STEP 8: RETURN SUCCESS
    // ─────────────────────────────────────────────────────────
    uart_puts("  Task PID ");
    uart_putdec(task->pid);
    uart_puts(" created successfully\n");
    
    return task->pid;  // Return the PID so caller can find task later
}
```

---

## File 3: exceptions.c - Exception Handling

### Exception Vector Table Setup

```c
void exceptions_init(void) {
    uart_puts("=== EXCEPTIONS & INTERRUPTS ===\n");
    
    // ─────────────────────────────────────────────────────────
    // STEP 1: INSTALL EXCEPTION VECTOR TABLE
    // ─────────────────────────────────────────────────────────
    // This is THE MOST IMPORTANT line for Phase 5!
    
    uart_puts("Installing exception vector table...\n");
    
    u64 vbar_addr = (u64)&exception_vector_table;
    // exception_vector_table is a global symbol defined in assembly
    // It points to 0x40081000 (where our handler code starts)
    
    uart_puts("  Exception vector table at: ");
    uart_puthex(vbar_addr);
    uart_puts("\n");
    
    // THIS LINE TELLS CPU WHERE TO FIND EXCEPTION HANDLERS!
    asm volatile("msr vbar_el1, %0" : : "r"(vbar_addr));
    //            ^^^                     ^^^
    //            msr = "Move to System Register"
    //            vbar_el1 = "Vector Base Address Register"
    
    // Now when any exception occurs:
    // 1. CPU reads VBAR_EL1 (which we just set to 0x40081000)
    // 2. CPU calculates handler offset based on exception type
    // 3. CPU jumps to VBAR_EL1 + offset
    // 4. Handler code, usually jumps to C function
    //
    // For SVC #0 from user mode:
    // - CPU calculates offset = 0x400 (Lower EL Synchronous)
    // - CPU jumps to 0x40081000 + 0x400 = 0x40081400
    // - At 0x40081400: assembly code that calls exc_svc_handler()
    
    uart_puts("  VBAR_EL1 register value: ");
    u64 read_vbar;
    asm volatile("mrs %0, vbar_el1" : "=r"(read_vbar));
    uart_puthex(read_vbar);
    uart_puts("\n");
    
    // ─────────────────────────────────────────────────────────
    // Rest of exception_init() initializes timer interrupts...
    // ─────────────────────────────────────────────────────────
}
```

### The Exception Vector Table (In Assembly)

```asm
asm(
    ".section .text.exceptions\n"  // Put this in .text section
    ".align 11\n"                   // Align to 2KB boundary (2^11)
    ".global exception_vector_table\n"
    "exception_vector_table:\n"     // This is the entry point

    /* CURRENT EL with SP0 - rarely used */
    /* 0x000-0x180: Skip (we use SPx below) */

    /* CURRENT EL with SPx - EL1 exceptions */
    /* 0x200: Synchronous (kernel exceptions) */
    "    str x30, [sp, #-16]!\n"    // Save return register
    "    bl exc_sync_handler\n"     // Branch to handler
    "    ldr x30, [sp], #16\n"      // Restore return register
    "    eret\n"                    // Return from exception
    ".align 7\n"                    // Pad to 128-byte boundary

    /* 0x280: IRQ (timer interrupt) */
    "    str x30, [sp, #-16]!\n"
    "    bl exc_irq_handler\n"      // Timer interrupt handler
    "    ldr x30, [sp], #16\n"
    "    eret\n"
    ".align 7\n"

    /* ... FIQ and SError handlers ... */

    /* LOWER EL (EL0 user mode) - THIS IS WHERE USER SYSCALLS GO! */
    /* 0x400: Synchronous (includes SVC instruction) */
    "    str x30, [sp, #-16]!\n"    // Save return register (x30)
    "    bl exc_svc_handler\n"      // Branch to SVC handler
    "    ldr x30, [sp], #16\n"      // Restore return register
    "    eret\n"                    // Return to user mode! ← CRITICAL
    ".align 7\n"

    /* ... rest of vector table ... */
);

void exc_svc_handler(void) {
    // ─────────────────────────────────────────────────────────
    // THIS FUNCTION EXECUTES IN KERNEL MODE (EL1)!
    // User code made an exception (SVC #0) and we're now
    // handling it in kernel mode
    // ─────────────────────────────────────────────────────────

    // STEP 1: READ SYSCALL NUMBER FROM x0
    // User code put syscall number in x0 before SVC #0
    u64 syscall_num;
    __asm__("mov %0, x0" : "=r"(syscall_num));
    // Read x0 register and store in syscall_num variable
    // For our test: syscall_num = 0 (default)

    // STEP 2: PRINT DEBUG MESSAGE
    uart_puts("[SYSCALL]\n");
    // Output: "[SYSCALL]"

    // STEP 3: DISPATCH TO SYSCALL HANDLER
    handle_syscall(syscall_num);
    // Route to appropriate syscall handler
    // (see handle_syscall function below)

    // ─────────────────────────────────────────────────────────
    // STEP 4: THIS IS CRITICAL - ADVANCE PC
    // ─────────────────────────────────────────────────────────
    // When SVC #0 raised exception, CPU saved the address of
    // the SVC instruction to ELR_EL1. If we don't advance it,
    // ERET will return to the same SVC instruction, causing
    // an infinite loop of exceptions!
    
    __asm__(
        "mrs x1, elr_el1 \n"      // Read Exception Link Register
                                  // x1 = 0x40100000 (SVC instruction)
        "add x1, x1, #4 \n"       // Add 4 (ARM64 instruction is 4 bytes)
                                  // x1 = 0x40100004 (next instruction)
        "msr elr_el1, x1 \n"      // Write back Exception Link Register
                                  // elr_el1 = 0x40100004 (next instruction)
    );
    
    // NOW when ERET executes:
    // PC will be set to 0x40100004 (next instruction after SVC)
    // NOT 0x40100000 (the SVC instruction)
    // Result: No infinite loop!
}

void handle_syscall(u64 syscall_num) {
    // ─────────────────────────────────────────────────────────
    // SYSCALL DISPATCHER
    // Different syscall numbers do different things
    // ─────────────────────────────────────────────────────────

    switch (syscall_num) {
        case 0x0:
            // Null syscall (default/test syscall)
            // Do nothing, just return
            break;
        
        case SYS_PRINT:  // 1
            // Print string (user put pointer in x1, length in x2)
            break;
        
        case SYS_EXIT:   // 2
            // Kill current task
            break;
        
        case SYS_GET_COUNTER:  // 3
            // Return tick count (return in x0)
            break;
        
        case SYS_SLEEP:  // 4
            // Sleep for N milliseconds
            break;
        
        case SYS_READ_CHAR:  // 5
            // Read character from keyboard
            break;
        
        default:
            uart_puts("[UNKNOWN SYSCALL]\n");
    }
}

void exc_irq_handler(void) {
    // ─────────────────────────────────────────────────────────
    // TIMER INTERRUPT HANDLER
    // Runs every 1ms (configured by exceptions_init)
    // ─────────────────────────────────────────────────────────
    
    tick_count++;  // Increment timer counter
    
    // Reload timer for next interrupt
    asm volatile("msr cntv_tval_el0, %0" : : "r"(timer_ticks_per_ms));
    
    // Print a dot every second (1000 ticks × 1ms = 1000ms)
    if (tick_count % 1000 == 0) {
        uart_puts(".");  // Visual indication that timer works
    }
}
```

---

## File 4: syscall.h - Syscall Interface

```c
/*
 * syscall.h - System Call Interface
 * 
 * This file defines the "contract" between user and kernel:
 * - What syscall numbers are available
 * - What arguments they expect
 * - What they return
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

/* ═══════════════════════════════════════════════════════════
   SYSCALL NUMBERS
   These are the "API" available to user programs
   User puts one of these in x0, then executes SVC #0
   ═══════════════════════════════════════════════════════════ */

#define SYS_PRINT       1    // Print string to console
#define SYS_EXIT        2    // Exit/kill current task
#define SYS_GET_COUNTER 3    // Get current tick count
#define SYS_SLEEP       4    // Sleep N milliseconds
#define SYS_READ_CHAR   5    // Read character from keyboard

/* ═══════════════════════════════════════════════════════════
   HOW SYSCALLS WORK (Example: SYS_PRINT)
   ═══════════════════════════════════════════════════════════
   
   USER CODE:
     mov x0, #SYS_PRINT      ; x0 = 1 (syscall number)
     mov x1, #message        ; x1 = pointer to string
     mov x2, #14             ; x2 = string length
     svc #0                  ; Execute! Raise exception!
   
   [Exception is raised, CPU enters EL1 (kernel mode)]
   
   KERNEL (exc_svc_handler):
     Read x0 (syscall_num = 1 = SYS_PRINT)
     handle_syscall(1)
       → Print message from x1 with length x2
       → Set return value in x0 (usually 0 = success)
     Advance PC
     ERET (return to user mode)
   
   [CPU returns to user mode at instruction after SVC]
   
   USER CODE CONTINUES:
     mov x3, x0              ; x3 = return value (0)
     (Next instruction)
*/

/* ═══════════════════════════════════════════════════════════
   EXCEPTION CLASSES (ESR_EL1 [31:26])
   These tell us what type of exception occurred
   ═══════════════════════════════════════════════════════════ */

#define EXC_SVC64       0x15    // SVC from 64-bit userspace
#define EXC_BRK64       0x31    // BRK instruction hit (breakpoint)

#endif /* SYSCALL_H */
```

---

## File 5: usermode.h - Data Structures

```c
/*
 * usermode.h - User Mode Data Structures
 * 
 * Defines the Task Control Block (TCB) that represents
 * each user task from the kernel's perspective
 */

#ifndef USERMODE_H
#define USERMODE_H

#include "types.h"

/* ═══════════════════════════════════════════════════════════
   USER TASK CONTROL BLOCK (TCB)
   
   This structure represents ONE user task from the kernel's view
   When a user task is running or paused, all its state is in here
   ═══════════════════════════════════════════════════════════ */

typedef struct {
    /* ─────────────────────────────────────────────────────
       IDENTIFICATION
       ───────────────────────────────────────────────────── */
    u32 pid;              // Process ID (1, 2, 3, etc)
    u32 state;            // DEAD, READY, RUNNING, or BLOCKED
    char name[32];        // Human-readable name ("Task_A", etc)
    
    /* ─────────────────────────────────────────────────────
       MEMORY LAYOUT
       
       Each task gets its own private memory:
       - Code:  Where user program instructions live
       - Stack: Where local variables and return addresses live
       - Heap:  Where malloc() allocations go
       ───────────────────────────────────────────────────── */
    
    u64 code_base;        // Start address of code page (e.g., 0x40100000)
    // Example Task A:
    //   code_base = 0x40100000
    //   Contains: SVC #0 followed by infinite loop
    
    u64 stack_base;       // Start address of stack page (e.g., 0x40101000)
    u64 stack_size;       // Stack size in bytes (4096 for Phase 5)
    // Stack grows downward, so:
    //   Highest address:   0x40101FFF
    //   Lowest address:    0x40101000
    //   SP starts at:      0x40101FF0 (slightly below top)
    
    u64 heap_base;        // Start address of heap page (e.g., 0x40102000)
    u64 heap_size;        // Heap size in bytes (4096 for Phase 5)
    // Heap grows upward (unused in Phase 5)
    
    /* ─────────────────────────────────────────────────────
       CPU STATE (used for context switching in Phase 4+)
       ───────────────────────────────────────────────────── */
    
    u64 pc;               // Program Counter (current instruction)
    u64 sp;               // Stack Pointer (top of user stack)
    u64 registers[31];    // x0-x30 (x31 is sp, used separately)
    
    /* ─────────────────────────────────────────────────────
       PRIVILEGE INFORMATION
       ───────────────────────────────────────────────────── */
    
    u32 exception_level;  // 0 = EL0 (user mode), 1 = EL1 (kernel mode)
    // In Phase 5, this is always 0 for user tasks
    
} user_task_t;

/* ═══════════════════════════════════════════════════════════
   TASK STATES
   ═══════════════════════════════════════════════════════════ */

#define TASK_DEAD        0    // Not in use, slot is free
#define TASK_READY       1    // Waiting to run
#define TASK_RUNNING     2    // Currently executing
#define TASK_BLOCKED     3    // Waiting for I/O or event

#endif /* USERMODE_H */
```

---

## Step-by-Step Execution Flow

### Timeline: What Happens When (In Sequence)

```
┌─────────────────────────────────────────────────────────────┐
│ TIME 0ms: CPU BOOTS                                         │
│ ─────────────────────────────────────────────────────────── │
│ • CPU executes firmware/bootloader                          │
│ • Firmware transitions to EL1                              │
│ • PC jumps to 0x40000000 (boot.S)                          │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 1ms: boot.S RUNS                                       │
│ ─────────────────────────────────────────────────────────── │
│ • Disable interrupts temporarily                            │
│ • Clear BSS section                                         │
│ • Set up kernel stack pointer (sp)                         │
│ • Jump to kernel_main()                                   │
└─────────────────────────────────────────────────────────────┘
         ↓ REGISTERS AT THIS POINT:
         │ • EL = 1 (kernel mode)
         │ • SP = kernel stack
         │ • PC = kernel_main function
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 2ms: kernel_main() BEGINS                              │
│ ─────────────────────────────────────────────────────────── │
│ • Call pmm_init()                                           │
│ • Call exceptions_init()                                   │
│   → VBAR_EL1 = 0x40081000 (exception vectors)             │
│ • Call usermode_init()                                    │
│ • Call create_user_task("Task_A", ...)                    │
│   → Allocate 3 pages: 0x40100000, 0x40101000, 0x40102000  │
│   → Fill code page with: SVC #0, b ., b ., ...            │
│   → Create TCB with pc=0x40100000, sp=0x40101FF0          │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 3ms: ALL TASKS CREATED                                 │
│ ─────────────────────────────────────────────────────────── │
│ • Task_A: Code 0x40100000, Stack 0x40101000                │
│ • Task_B: Code 0x40103000, Stack 0x40104000                │
│ • Task_C: Code 0x40106000, Stack 0x40107000                │
│ • Kernel ready to switch to user mode                      │
└─────────────────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 4ms: CALLING switch_to_user_mode(task_a)              │
│ ─────────────────────────────────────────────────────────── │
│ • Pass pointer to Task_A TCB (contains pc=0x40100000)      │
│ • Call switch_to_user_mode_asm()                           │
│ • Load:  ELR_EL1 = 0x40100000 (user PC)                   │
│ • Load:  SPSR_EL1 = 0 (EL0)                               │
│ • Load:  SP = 0x40101FF0 (user stack)                     │
│ • Execute: ERET                                            │
└─────────────────────────────────────────────────────────────┘
         ↓ ← PRIVILEGE LEVEL CHANGES HERE ←
         │   EL1 → EL0
         ↓ REGISTERS AT THIS POINT:
         │ • EL = 0 (USER MODE!)
         │ • SP = 0x40101FF0 (user stack)
         │ • PC = 0x40100000 (user code!)
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 5ms: USER CODE EXECUTES (in EL0)                      │
│ ─────────────────────────────────────────────────────────── │
│ • CPU fetches instruction at PC=0x40100000                 │
│ • Instruction: 0xD4000001 (SVC #0)                        │
│ • CPU executes SVC #0 instruction                         │
│ • SVC #0 raises exception!                                │
│   → CPU saves state to ELR_EL1=0x40100000                 │
│   → CPU saves privilege to SPSR_EL1=0                     │
│   → CPU reads VBAR_EL1 = 0x40081000                       │
│   → CPU calculates handler offset = 0x400 (Lower EL Sync) │
│   → CPU jumps to 0x40081000 + 0x400 = 0x40081400         │
└─────────────────────────────────────────────────────────────┘
         ↓ ← EXCEPTION RAISED ←
         │   EL0 → EL1
         ↓ REGISTERS AT THIS POINT:
         │ • EL = 1 (KERNEL MODE AGAIN!)
         │ • ELR_EL1 = 0x40100000 (saved user PC)
         │ • SPSR_EL1 = 0 (saved privilege: EL0)
         │ • PC = 0x40081400 (exception handler)
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 6ms: EXCEPTION HANDLER RUNS (in EL1)                   │
│ ─────────────────────────────────────────────────────────── │
│ • At 0x40081400: Assembly code                             │
│   str x30, [sp, #-16]!   ; Save return register           │
│   bl exc_svc_handler     ; Call handler                   │
│                                                             │
│ • exc_svc_handler() executes:                              │
│   - Read x0 (syscall_num = 0)                             │
│   - Print "[SYSCALL]"                                    │
│   - Call handle_syscall(0)                               │
│   - Read ELR_EL1 = 0x40100000                            │
│   - Add 4: ELR_EL1 = 0x40100004                          │
│   - Write back ELR_EL1                                    │
│   - Return                                                │
│                                                             │
│ • Back at 0x40081400: Assembly code                       │
│   ldr x30, [sp], #16     ; Restore return register       │
│   eret                   ; RETURN TO USER MODE!           │
└─────────────────────────────────────────────────────────────┘
         ↓ ← RETURNING TO USER MODE ←
         │   EL1 → EL0
         ↓ REGISTERS AT THIS POINT:
         │ • EL = 0 (USER MODE!)
         │ • PC = ELR_EL1 = 0x40100004 (NEXT instruction!)
         │ • SP = 0x40101FF0 (user stack)
         ↓
┌─────────────────────────────────────────────────────────────┐
│ TIME 7ms: USER CODE CONTINUES (in EL0)                      │
│ ─────────────────────────────────────────────────────────── │
│ • CPU fetches instruction at PC=0x40100004                 │
│ • Instruction: 0x14000000 (b . - infinite loop)           │
│ • CPU executes: branch to self (loop forever)             │
│ • Keeps executing the same instruction at 0x40100004      │
│                                                             │
│ RESULT: STABLE! No more exceptions!                        │
│ User code happy, kernel happy, system stable!             │
└─────────────────────────────────────────────────────────────┘
```

---

## Summary: How It All Works

1. **Boot:** CPU starts, runs boot.S, enters kernel_main in EL1
2. **Init:** Kernel initializes PMM, exceptions, user mode system
3. **Create:** Kernel creates user tasks, allocates memory
4. **Switch:** Kernel loads user PC/SP into ELR_EL1/SP, ERET to EL0
5. **User:** User code executes, hits SVC #0 instruction
6. **Exception:** CPU enters exception handler at [0x400]
7. **Handler:** Kernel processes syscall, advances PC
8. **Return:** ERET returns to user mode at next instruction
9. **Loop:** User code continues, may loop or make more syscalls

This cycle enables secure communication between user and kernel!

