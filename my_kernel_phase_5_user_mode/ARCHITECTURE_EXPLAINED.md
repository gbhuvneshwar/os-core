# Phase 5 Architecture: Complete Step-by-Step Explanation

## Table of Contents
1. [Overall Architecture](#overall-architecture)
2. [Memory Layout](#memory-layout)
3. [Privilege Levels](#privilege-levels)
4. [Boot Sequence](#boot-sequence)
5. [User Task Creation](#user-task-creation)
6. [Privilege Switching](#privilege-switching)
7. [System Call Execution](#system-call-execution)
8. [Exception Handling](#exception-handling)

---

## Overall Architecture

### Three Execution Domains

```
┌─────────────────────────────────────────────────────────┐
│  KERNEL SPACE (EL1 - Kernel Mode)                      │
│  ─────────────────────────────────────────────────────  │
│  • boot.S          - Bare metal boot                     │
│  • kernel.c        - Initialization & scheduling         │
│  • memory.c        - Physical memory management (PMM)    │
│  • exceptions.c    - Exception handling                  │
│  • uart.c          - Serial I/O driver                   │
│                                                          │
│  CAPABILITIES (EL1):                                    │
│  ✓ Modify CPU registers (VBAR_EL1, SPSR_EL1, etc)      │
│  ✓ Access all hardware (timer, UART, memory)           │
│  ✓ Modify page tables (when MMU enabled)               │
│  ✓ Execute privileged instructions                      │
│  ✓ Access any memory location                           │
└─────────────────────────────────────────────────────────┘
           ↕ Exception/ERET
┌─────────────────────────────────────────────────────────┐
│  USER SPACE (EL0 - User Mode)                           │
│  ─────────────────────────────────────────────────────  │
│  • Task A (PID 1)  - User program                       │
│  • Task B (PID 2)  - User program                       │
│  • Task C (PID 3)  - User program                       │
│                                                          │
│  CAPABILITIES (EL0):                                    │
│  ✓ Execute basic instructions (arithmetic, logic)      │
│  ✓ Access own memory (code, stack, heap)               │
│  ✓ Execute SVC instruction (syscall)                   │
│  ✗ Cannot modify CPU registers (no MSR)                │
│  ✗ Cannot access hardware directly                     │
│  ✗ Cannot access kernel memory                         │
│  ✗ Cannot execute privileged instructions              │
└─────────────────────────────────────────────────────────┘
```

### Key ARM64 Registers Used

| Register | Purpose | Accessed By |
|----------|---------|-------------|
| **VBAR_EL1** | Exception Vector Base Address (pointer to [0x400] handlers) | Kernel only (EL1) |
| **ELR_EL1** | Exception Link Register (stores return address during exception) | Kernel only (EL1) |
| **SPSR_EL1** | Saved Processor State Register (stores privilege level bits [3:0]) | Kernel only (EL1) |
| **SP** | Stack Pointer (kernel stack at EL1, user stack at EL0) | Both |
| **x0-x7** | General purpose registers (function arguments) | Both |

---

## Memory Layout

### Complete Physical Memory Map

```
0x40000000 ┌─────────────────────────────────────┐
           │  KERNEL CODE & DATA                 │
           │  • boot.S (0x40000000)              │
           │  • kernel.c code (0x40000168+)      │
           │  • Global variables                 │
           │  • Kernel BSS section               │
           │                                     │
0x40081000 ├─────────────────────────────────────┤ ← VBAR_EL1
           │  EXCEPTION VECTOR TABLE             │
           │  • [0x000] Current EL sync          │
           │  • [0x200] Current EL IRQ (timer)   │
           │  • [0x400] Lower EL sync (SVC!)    │
           │  • [0x480] Lower EL IRQ             │
           │                                     │
0x40090000 ├─────────────────────────────────────┤
           │  KERNEL HEAP                        │
           │  (Dynamic memory allocations)       │
           │                                     │
0x40100000 ├─────────────────────────────────────┤
           │  USER TASK A                        │
           │  • Code:  0x40100000-0x40100FFF     │
           │  • Stack: 0x40101000-0x40101FFF     │
           │  • Heap:  0x40102000-0x40102FFF     │
           │                                     │
0x40103000 ├─────────────────────────────────────┤
           │  USER TASK B                        │
           │  • Code:  0x40103000-0x40103FFF     │
           │  • Stack: 0x40104000-0x40104FFF     │
           │  • Heap:  0x40105000-0x40105FFF     │
           │                                     │
0x40106000 ├─────────────────────────────────────┤
           │  USER TASK C                        │
           │  • Code:  0x40106000-0x40106FFF     │
           │  • Stack: 0x40107000-0x40107FFF     │
           │  • Heap:  0x40108000-0x40108FFF     │
           │                                     │
0x40109000 ├─────────────────────────────────────┤
           │  FREE MEMORY                        │
           │  (32,512 free pages remaining)      │
           │                                     │
0x48000000 └─────────────────────────────────────┘
```

### Why This Layout Matters

1. **Separation:**
   - Kernel memory at bottom (0x40000000)
   - User tasks separated from kernel
   - Each user task isolated from others

2. **Protection:**
   - User code cannot reach kernel memory
   - User task A cannot reach task B's memory
   - CPU privilege level enforces this via exception on illegal access

3. **Scalability:**
   - Can add more user tasks by allocating more 4KB triplets
   - Hundreds of tasks possible with 128MB RAM

---

## Privilege Levels

### ARM64 Exception Levels

```
EL3 ─────────────────────────
     Secure Monitor
     (Not used in QEMU virt)

EL2 ─────────────────────────
     Hypervisor/Virtual Machine Monitor
     (Not used in QEMU virt)

EL1 ─────────────────────────  ← OUR KERNEL (PHASE 5)
     Kernel Mode              
     • Full hardware access
     • Can modify privilege registers
     • Exception handling
     • System call processing

EL0 ─────────────────────────  ← USER PROGRAMS (PHASE 5)
     User Mode
     • Restricted instruction set
     • Cannot access hardware
     • Can only make syscalls
     • Isolated memory
```

### SPSR_EL1 Register: Privilege Control

The **SPSR_EL1** (Saved Processor State Register) controls privilege level when ERET executes:

```
SPSR_EL1 bits [3:0] = Exception Level to return to:
  0b00 = EL0  (User mode)     ← Set this to switch to user
  0b01 = EL1  (Kernel mode)   ← Our kernel runs here
  0b10 = EL2  (Hypervisor)
  0b11 = EL3  (Secure monitor)

When ERET executes:
1. Read SPSR_EL1[3:0]
2. If 0b00: Transition to EL0 (user mode)
3. If 0b01: Stay in EL1 (kernel mode)
4. Jump to address in ELR_EL1
```

### Privilege Check in Hardware

When user code tries privileged operation:
```
User Code (EL0):
  mov x0, #0
  msr spsr_el1, x0   ← ILLEGAL! User cannot write SPSR_EL1

CPU checks:
  IF (current_EL == 0 AND instruction_requires_EL1) {
    RAISE EXCEPTION to EL1 handler
  }
```

---

## Boot Sequence

### Step 1: Power On → EL1 Kernel Mode

```
1. ARM CPU boots at 0x40000000
   ├─ Starts in EL3 (secure monitor)
   └─ Firmware/bootloader transitions to EL1

2. boot.S executes:
   ├─ .section ".text.boot"
   ├─ .global _start
   ├─ Disable IRQ/FIQ interrupts
   ├─ Clear BSS section
   ├─ Set up kernel stack
   └─ Jump to kernel_main()

3. Kernel is now in EL1 (kernel mode)
   ├─ Can execute privileged instructions
   ├─ Can access all memory
   └─ Can modify CPU registers
```

### Step 2: Kernel Initialization (kernel.c)

```c
void kernel_main(void) {
    uart_puts("╔═════════════════════════════╗\n");
    uart_puts("║  PHASE 5: USER MODE & SYSCALLS ║\n");
    uart_puts("╚═════════════════════════════╝\n");
    
    // 1. Initialize Physical Memory Manager
    pmm_init();
    // → Discovers 32,512 free pages (130MB)
    // → Tracks which pages are free vs allocated
    
    // 2. Install Exception Vectors
    exceptions_init();
    // → Sets VBAR_EL1 = 0x40081000
    // → CPU now knows where exception handlers are
    // → Exception at offset [0x400] will jump to exc_svc_handler
    
    // 3. Initialize User Mode System
    usermode_init();
    // → Creates empty user task pool
    // → Ready to create user tasks
    
    // 4. Create User Tasks
    create_user_task("Task_A", NULL, 4096, 4096);
    create_user_task("Task_B", NULL, 4096, 4096);
    create_user_task("Task_C", NULL, 4096, 4096);
    // → Each task gets:
    //   - 4KB code page (filled with SVC instruction + loop)
    //   - 4KB stack page (SP at top-16)
    //   - 4KB heap page (unused for now)
    
    // 5. Switch to User Mode
    switch_to_user_mode(&user_tasks[0]);
    // → This call NEVER RETURNS (by design)
    // → CPU enters EL0 user mode
    // → Execution continues in user code
}
```

---

## User Task Creation

### Step 1: PMM Allocation

When `create_user_task()` is called:

```c
// Allocate code memory (must be page-aligned)
task->code_base = pmm_alloc_page();
// → PMM finds free page (0x40100000)
// → Marks page as allocated
// → Returns address 0x40100000

// Allocate stack memory
task->stack_base = pmm_alloc_page();
// → PMM finds free page (0x40101000)
// → Returns address 0x40101000

// Allocate heap memory
task->heap_base = pmm_alloc_page();
// → PMM finds free page (0x40102000)
// → Returns address 0x40102000
```

### Step 2: Fill Code Page with User Code

```c
u32 *code_ptr = (u32 *)task->code_base;
// code_ptr now points to user code page (0x40100000)

code_ptr[0] = 0xD4000001;    // SVC #0 instruction
// This is the ARM64 encoding for SVC (Supervisor Call)
// When executed, causes exception to EL1

for (int i = 1; i < 1024; i++) {
    code_ptr[i] = 0x14000000; // b . (branch to self)
}
// Infinite loop: b . branches to itself forever
```

### Step 3: Initialize Task Control Block

```c
task->pid = 1;                          // Process ID
task->state = TASK_DEAD;               // Will change to RUNNING
task->pc = task->code_base;            // PC = 0x40100000 (start of code)
task->sp = task->stack_base + 4096 - 16; // SP = 0x40101FF0
strcpy(task->name, "Task_A");          // Name for debugging
```

### Step 4: Result

```
After create_user_task("Task_A"):

Memory:
  0x40100000: [0xD4000001] [0x14000000] [0x14000000] ...  ← Code page
             SVC #0       branch loop   branch loop
             
  0x40101000: [garbage]   [garbage]   ...                  ← Stack page
  0x40101FF0: [empty]     [empty]     ...   (SP points here)
  
TCB (Task Control Block):
  pid = 1
  pc = 0x40100000
  sp = 0x40101FF0
  code_base = 0x40100000
  stack_base = 0x40101000
```

---

## Privilege Switching

### The Critical Moment: ERET

The switch from EL1 (kernel) to EL0 (user) happens in `switch_to_user_mode_asm()`:

```asm
switch_to_user_mode_asm:
    # x0 = pointer to user_task_t structure
    
    ldr x1, [x0, #48]      # Load user PC from task->pc
    ldr x2, [x0, #56]      # Load user SP from task->sp
    
    msr elr_el1, x1        # Set "exception link register" = user PC
                           # This is where CPU will jump after ERET
    
    mov x3, #0            # x3 = 0
    msr spsr_el1, x3      # Set SPSR_EL1 = 0 (means EL0)
                          # This tells CPU: when ERET, go to EL0
    
    mov sp, x2            # Load user stack pointer
                          # Now kernel stack is gone!
    
    eret                  # MAGIC HAPPENS HERE!
                          # CPU reads SPSR_EL1[3:0] = 0 (EL0)
                          # CPU reads ELR_EL1 = 0x40100000
                          # Privilege level changes: EL1 → EL0
                          # PC jumps to 0x40100000
                          # Execution continues in user code
```

### What Happens During ERET

```
BEFORE ERET (in EL1, kernel mode):
  Current EL = 1 (kernel)
  PC = 0x40008234 (kernel code)
  SP = kernel stack
  Can execute: msr, mrs, privileged instructions

ERET EXECUTES:
  1. Reads SPSR_EL1 bits [3:0] = 0
  2. Sets Current EL = 0
  3. Reads ELR_EL1 = 0x40100000
  4. Sets PC = 0x40100000
  5. Sets SP = 0x40101FF0 (already set by mov sp, x2)

AFTER ERET (in EL0, user mode):
  Current EL = 0 (user)
  PC = 0x40100000 (user code!)
  SP = 0x40101FF0 (user stack)
  Cannot execute: msr, mrs, privileged instructions
  
  Next instruction:
  At address 0x40100000: SVC #0
  User code executes: SVC #0
```

### Why This Is Secure

1. **CPU Enforces Privilege:**
   - If user code tries `msr spsr_el1, x0`, CPU raises exception
   - Kernel exception handler catches it
   - User cannot escalate privilege

2. **Memory Is Isolated:**
   - User code at 0x40100000 cannot read/write 0x40000000 (kernel)
   - On attempt, CPU raises exception
   - Kernel handles the fault

3. **One-Way Switch:**
   - Only ERET (from EL1) can switch back to EL1
   - User code cannot execute ERET (would raise exception)
   - User must use SVC to call kernel

---

## System Call Execution

### Step 1: User Code Executes SVC

```
User Code (EL0):
  At address 0x40100000:
  
  Instruction: 0xD4000001 (SVC #0)
  
  CPU execution:
  1. Decode SVC #0
  2. Detect: Current EL = 0, instruction requires EL1 transition
  3. Save current state
  4. Raise synchronous exception
```

### Step 2: Exception Vectors Route It

```
Exception Raised:
  Exception Type: Lower EL (from EL0) Synchronous (SVC)
  CPU reads: VBAR_EL1 = 0x40081000
  
  VBAR_EL1 + 0x400 = 0x40081400
  
  Jump to handler code at 0x40081400:
  
  asm code:
    str x30, [sp, #-16]!        # Save return register
    bl exc_svc_handler          # Call handler
    ldr x30, [sp], #16          # Restore
    eret                        # Return to user mode
```

### Step 3: Exception Handler Processes It

```c
void exc_svc_handler(void) {
    // We're now in EL1 (kernel mode)
    // User code is frozen
    
    // Step 1: Read syscall number from x0
    u64 syscall_num;
    __asm__("mov %0, x0" : "=r"(syscall_num));
    // syscall_num = 0 (user put 0 in x0 before SVC)
    
    // Step 2: Print debug message
    uart_puts("[SYSCALL]\n");
    // Output: "[SYSCALL]"
    
    // Step 3: Dispatch to syscall handler
    handle_syscall(syscall_num);
    // Calls syscall dispatcher (see next section)
    
    // Step 4: THIS IS CRITICAL - Advance PC
    __asm__(
        "mrs x1, elr_el1 \n"    # Read exception link register
                                # x1 = 0x40100000 (SVC instruction)
        "add x1, x1, #4 \n"     # Add 4 (size of ARM64 instruction)
                                # x1 = 0x40100004 (next instruction)
        "msr elr_el1, x1 \n"    # Write back to exception link register
                                # ELR_EL1 = 0x40100004
    );
    // WITHOUT THIS: ERET would jump back to SVC (infinite loop!)
    // WITH THIS: ERET jumps to next instruction (b . loop)
}
```

### Step 4: Syscall Dispatcher

```c
void handle_syscall(u64 syscall_num) {
    switch (syscall_num) {
        case 0x0:  // Null syscall (do nothing)
            break;
        case SYS_PRINT:     // Print string
            // x1 = pointer, x2 = length
            break;
        case SYS_EXIT:      // Kill task
            // x1 = exit code
            break;
        // ... more syscalls
    }
}
```

### Step 5: Return to User Mode

```
Handler finishes
  ↓
Assembly at [0x400]:
  ldr x30, [sp], #16    # Restore return register
  eret                  # RETURN TO USER MODE
  
ERET EXECUTES:
  1. Reads ELR_EL1 = 0x40100004 (PC advanced by handler!)
  2. Reads SPSR_EL1[3:0] = 0 (still EL0)
  3. Sets PC = 0x40100004
  4. Privilege level changes: EL1 → EL0
  
User Code Resumes:
  PC = 0x40100004 (next instruction after SVC)
  Instruction: 0x14000000 (b . loop)
  
  Result: Infinite loop, no more exceptions!
```

---

## Exception Handling

### The Exception Vector Table

Located at 0x40081000 (set by VBAR_EL1):

```
Offset  │ EA Bits [5:3] │ Exception Type         │ Handler
─────────┼───────────────┼────────────────────────┼──────────────
0x000    │     000       │ Current EL SP0 Sync    │ brk #1 (unused)
0x080    │     010       │ Current EL SP0 IRQ     │ brk #1 (unused)
0x100    │     100       │ Current EL SP0 FIQ     │ brk #1 (unused)
0x180    │     110       │ Current EL SP0 SError  │ brk #1 (unused)
─────────┼───────────────┼────────────────────────┼──────────────
0x200    │     000       │ Current EL SPx Sync    │ exc_sync_handler
0x280    │     010       │ Current EL SPx IRQ     │ exc_irq_handler (timer!)
0x300    │     100       │ Current EL SPx FIQ     │ exc_fiq_handler
0x380    │     110       │ Current EL SPx SError  │ exc_serror_handler
─────────┼───────────────┼────────────────────────┼──────────────
0x400    │     000       │ LOWER EL Sync (SVC!)   │ exc_svc_handler ← USER SYSCALLS
0x480    │     010       │ LOWER EL IRQ           │ brk #1 (unused)
0x500    │     100       │ LOWER EL FIQ           │ brk #1 (unused)
0x580    │     110       │ LOWER EL SError        │ brk #1 (unused)
```

### Why Offset 0x400?

```
Exception at Lower EL (EL0) Synchronous:
  Exception Syndrome Register EA bits [5:3] = 0b000 (Synchronous)
  Exception from Lower EL = LOWER EL offset
  
  VBAR_EL1 + (EA[5:3] << 7) + (LOWER_EL << 7)
  = 0x40081000 + (0 << 7) + (4 << 6)
  = 0x40081000 + 0 + 0x400
  = 0x40081400
  
  But actual handler code is at 0x40081400-0x40081421
  This is the SVC handler for user syscalls!
```

---

## Complete Execution Timeline

### From Power On to User Mode: Full Trace

```
TIME    LOCATION        PRIVILEGE    CODE                    STATE
──────────────────────────────────────────────────────────────────────────
 0ms    0x40000000      EL1          boot.S _start           CPU powers on
        
 1ms    0x40000168      EL1          kernel_main             PMM init
        
 2ms    0x40081000      EL1          VBAR_EL1 set           Exception vectors ready
        
 3ms    0x40000xxx      EL1          create_user_task        Tasks created
        0x40100000: SVC | loop
        0x40101FF0: (user SP)
        
 4ms    0x40008xxx      EL1          switch_to_user_mode_asm About to switch
        
 4ms    ERET            → PRIVILEGE CHANGE EL1 → EL0 ←
        
 5ms    0x40100000      EL0          SVC #0                 User code executing!
        
 6ms    0x40081400      EL1          exc_svc_handler        Exception raised
                                     "[SYSCALL]" print
        
 7ms    0x40100004      EL1          PC advance logic        PC = 0x40100004
        
 7ms    ERET            → PRIVILEGE CHANGE EL1 → EL0 ←
        
 8ms    0x40100004      EL0          b . (loop)             User continues!
        
 9ms    0x40100004      EL0          b . (loop)             Looping forever
        
10ms    0x40100004      EL0          b . (loop)             No more interrupts!
```

---

## Key Takeaways

✅ **Phase 5 Demonstrates:**

1. **Two Execution Modes**
   - EL1 = Kernel (full power)
   - EL0 = User (limited, safe)

2. **Secure Transitions**
   - ERET controlled by SPSR_EL1
   - User cannot escalate privilege
   - Only syscalls allowed to reach kernel

3. **Memory Isolation**
   - Each task has separate pages
   - User code cannot read kernel memory
   - CPU enforces privilege in hardware

4. **Exception-Based Communication**
   - SVC instruction raises exception
   - Kernel exception handler processes it
   - Returns via ERET to user mode

5. **The Foundation**
   - This is core OS architecture
   - Real systems (Linux, Windows) use same model
   - Enables multitasking safely

---

## Next Steps: Phase 6

With Phase 5 complete, Phase 6 will add:
- **Virtual Memory (MMU)**
- **Page Tables** (translate virtual → physical)
- **Memory Protection** (hardware-enforced isolation)

This further strengthens security by making memory isolation invisible to user code!

