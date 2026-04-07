# ARM64 Privilege Levels: User Mode vs Kernel Mode

## What is User Mode vs Kernel Mode?

Operating systems run code at different **privilege levels** to protect the system:

```
┌─────────────────────────────────────────┐
│         USER MODE (EL0)                 │
│                                         │
│  ├─ User applications                   │
│  ├─ User programs                       │
│  ├─ Web browsers                        │
│  └─ Limited access to hardware          │
│                                         │
│  Can do:                                │
│    ✅ Read/Write own memory             │
│    ✅ Call system calls                 │
│    ✅ Execute most instructions         │
│                                         │
│  Cannot do:                             │
│    ❌ Access other process memory       │
│    ❌ Modify page tables                │
│    ❌ Disable interrupts                │
│    ❌ Access privileged registers       │
│    ❌ Access hardware directly          │
│                                         │
│                    ↓ SYSCALL (SVC)      │
├─────────────────────────────────────────┤
│       KERNEL MODE (EL1)                 │
│                                         │
│  ├─ OS kernel                           │
│  ├─ Device drivers                      │
│  ├─ Memory management                   │
│  ├─ Process scheduling                  │
│  └─ Full hardware access                │
│                                         │
│  Can do:                                │
│    ✅ Access ALL memory                 │
│    ✅ Modify page tables                │
│    ✅ Disable interrupts                │
│    ✅ Access privileged registers       │
│    ✅ Control hardware devices          │
│    ✅ Switch privilege levels           │
│                                         │
└─────────────────────────────────────────┘
```

## Why Do We Need User Mode?

### Problem Without User Mode

If all programs run in kernel mode:

```
Program A                Program B
(malicious)              (banking)

Can do:
❌ Crash the entire OS
❌ Read Program B's memory
❌ Modify page tables
❌ Access all hardware
❌ Turn off security

Result: SYSTEM DISASTER!
```

### Solution: User Mode Privilege Separation

With user mode:

```
Program A (User Mode)    Program B (User Mode)
(malicious)              (banking)

Can do:
✅ Only access own memory
✅ Only do system calls
✅ Cannot access other programs
✅ Cannot modify kernel
✅ Cannot crash OS directly

If Program A crashes:
  OS continues running
  Program B unaffected
  Program A killed cleanly
```

## ARM64 Exception Levels (EL0-EL3)

ARM64 has 4 privilege levels:

```
┌────────────────────────────────────┐
│ EL3: Secure Monitor                │
│ ├─ Firmware                        │
│ └─ Highest privilege               │
├────────────────────────────────────┤
│ EL2: Hypervisor                    │
│ ├─ Virtual machines                │
│ ├─ Hypervisor features             │
│ └─ Between kernel and secure       │
├────────────────────────────────────┤
│ EL1: Kernel Mode (what we use)     │
│ ├─ Operating system                │
│ ├─ Device drivers                  │
│ └─ Full hardware access            │
├────────────────────────────────────┤
│ EL0: User Mode (what we use)       │
│ ├─ User applications               │
│ └─ Limited access                  │
└────────────────────────────────────┘

In this implementation:
- EL1 = Kernel mode (our OS)
- EL0 = User mode (user programs)
- EL2 and EL3 = Not used by QEMU virt
```

## Key Registers for Privilege Control

### SPSR_EL1 (Saved Program Status Register)

When switching to user mode, we set:

```
SPSR_EL1 register (32-bit):
┌─────────────────────────────────┐
│Bit 0: M[0] = 0 (user mode)      │  0000 = EL0
│Bit 1: M[1] = 0                  │
│Bit 2: M[2] = 0                  │
│Bit 3: M[3] = 0                  │
│       ...                       │
│Bit 6: F = 0 (IRQ enabled)       │
│Bit 7: I = 0 (FIQ enabled)       │
│       ...                       │
│Bit 9: A = 0 (async abort on)    │
└─────────────────────────────────┘

Key bits:
- M[3:0] = Exception level (0000 = EL0, 0001 = EL1)
- F = FIQ interrupt disable
- I = IRQ interrupt disable
- A = Async abort disable
```

### SCTLR_EL1 (System Control Register)

Controls various CPU behaviors:

```
Important bits:
- M bit (0): MMU enabled/disabled
- A bit (1): Alignment check
- C bit (2): Cache enabled/disabled
- SA bit (3): Stack alignment check
- I bit (12): Instruction cache enabled
```

## The SVC Instruction: System Call

When user mode code needs kernel services, it uses **SVC** (Supervisor Call):

```
User Mode Code:
┌───────────────────────────────┐
│  // I need to print a message │
│  // to serial (kernel job)   │
│                               │
│  // Set up syscall number     │
│  mov x0, 1024  // SYS_PRINT   │
│  mov x1, msg   // syscall arg │
│                               │
│  svc #0        // <-- SYSCALL │
│  ^             ^              │
│  └─ Software   └─ ID (usually │
│     interrupt     0 for linux)│
└───────────────────────────────┘
           ↓
    Exception to EL1
           ↓
Kernel Exception Handler:
┌───────────────────────────────┐
│  esr = read ESR_EL1           │
│  exc_class = esr >> 26        │
│  if (exc_class == 0x15)       │
│    // SVC exception!          │
│    service_number = x0        │
│    syscall_handler(service)   │
└───────────────────────────────┘
           ↓
    Handle syscall
           ↓
    // Return to user mode
    eret  // Exception return
           ↓
User Mode Code Continues
```

## Exception Classes in ARM64

When an exception happens, ESR_EL1 contains the exception class:

```
ESR_EL1[31:26] = Exception Class

Value  | Name              | What it is
-------|-------------------|----------------------------------
0x00   | UNKNOWN           | Unknown exception
0x01   | WFIE              | Wait for interrupt exception
0x03   | CP15 (MCR/MRC)    | CP15 instruction
0x07   | CP15 (MCRR/MRRC)  | CP15 register pair
0x0C   | CP14 (MCR/MRC)    | CP14 instruction
0x0D   | CP14 (LDC/STC)    | CP14 instruction
0x0E   | SIMD              | SIMD floating-point
0x0F   | IME               | Illegal instruction
0x11   | SVC32             | SVC in 32-bit state
0x12   | HVC               | Hypervisor call
0x13   | SMC               | Secure monitor call
0x14   | SVC64             | SVC in 64-bit state (OUR SYSCALL!)
0x15   | HVC64             | Hypervisor call (64-bit)
0x16   | SMC64             | Secure monitor call (64-bit)
0x18   | Instr abort       | Instruction abort
...various others...
0x20-0x23 | Data abort     | Data abort (memory access)
0x24   | Stack overflow    | Stack pointer misalignment
...more...
```

For our syscalls: **When SVC executed, ESR_EL1[31:26] = 0x15** (SVC64)

## Register Conventions for System Calls

When calling a syscall:

```
┌─────────────────────────────────────┐
│  System Call Calling Convention     │
├─────────────────────────────────────┤
│ x0 = Syscall Number / Return value  │
│ x1 = First argument                 │
│ x2 = Second argument                │
│ x3 = Third argument                 │
│ x4 = Fourth argument                │
│ x5 = Fifth argument                 │
│ x6 = Sixth argument                 │
│ x7 = Seventh argument               │
│                                     │
│ Return from kernel:                 │
│ x0 = Return value                   │
│ x1-x7 may be modified               │
└─────────────────────────────────────┘

Example: sys_print(const char *str, int len)

User Code:
  mov x0, 1        // SYS_PRINT
  mov x1, str_ptr  // arg 1: string pointer
  mov x2, len      // arg 2: length
  svc #0           // syscall

Kernel:
  // x0 = 1 (SYS_PRINT)
  // x1 = pointer to string
  // x2 = string length
  handle_syscall()
  // Returns with x0 = status
```

## Flow: From User Mode to Kernel and Back

```
┌──────────────────────────────────────────────────────────────┐
│ INITIALIZATION (Kernel Mode EL1)                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ 1. Set up exception vector at VBAR_EL1                      │
│    This table handles all exceptions                        │
│                                                              │
│ 2. Create user mode task in memory                          │
│    Allocate stack, set up registers                         │
│                                                              │
│ 3. Set up SPSR_EL1 for EL0 (user mode)                      │
│    Bit pattern that says "run at EL0"                       │
│                                                              │
│ 4. Load user task PC into ELR_EL1                           │
│    (where user code will start)                             │
│                                                              │
│ 5. Execute "eret" (Exception Return)                        │
│    Jumps to user task, switches to EL0                      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────────┐
│ USER MODE CODE RUNNING (EL0)                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ void user_task() {                                          │
│   // Can do normal stuff                                    │
│   int x = 5 + 3;                                            │
│                                                              │
│   // When need kernel service:                              │
│   sys_print("Hello!");  // Function that does svc #0       │
│                                                              │
│ }                                                            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                         ↓
                    SVC #0 (syscall)
                         ↓ 
              Exception to EL1 (automatic)
                         ↓
┌──────────────────────────────────────────────────────────────┐
│ EXCEPTION HANDLER IN KERNEL (EL1)                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ svc_exception_handler() {                                   │
│   // x0 = syscall number (from user code)                   │
│   // x1-x7 = arguments                                      │
│                                                              │
│   switch(x0) {                                              │
│     case SYS_PRINT:                                         │
│       len = x2;       // Get from user                      │
│       uart_puts(x1);  // Call kernel function              │
│       x0 = 0;         // Return success                     │
│       break;                                                │
│                                                              │
│     case SYS_EXIT:                                          │
│       // Clean up task                                      │
│       break;                                                │
│   }                                                          │
│                                                              │
│   // Execute eret to return to user mode                   │
│   // Return values in x0                                    │
│                                                              │
│ }                                                            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                         ↓
                   ERET instruction
                  Returns to EL0 user
                         ↓
┌──────────────────────────────────────────────────────────────┐
│ USER MODE CODE CONTINUES (EL0)                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   // Back in user code, syscall complete                    │
│   // x0 has return value (0 = success)                      │
│                                                              │
│ }  // user_task ends                                        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Real Examples

### Example 1: Print String (Syscall)

**User code:**
```c
void user_task() {
    // Call sys_print to print via kernel
    sys_print("User says hello!\n");
}
```

**Expands to assembly:**
```asm
mov x0, #1           // SYS_PRINT
adr x1, msg          // pointer to string
mov x2, #18          // length
svc #0               // <-- Exception! Jump to kernel
// Return here when done
```

**Kernel handler:**
```c
// In exception handler
if (exc_class == SVC64) {
    switch(x0) {  // x0 = 1 (SYS_PRINT)
        case 1:
            uart_puts((char*)x1);  // Print string
            x0 = 0;  // Return success
            break;
    }
}
```

### Example 2: Exit (Syscall)

**User code:**
```c
void user_task() {
    uart_puts("Doing work...\n");
    sys_exit(0);  // Exit with status 0
}
```

**Kernel handles:**
```c
case SYS_EXIT:
    task->state = DEAD;
    // Don't return to user - schedule next task
    break;
```

## Memory Protection in User Mode

With MMU and privilege levels:

```
Physical Memory:

┌──────────────────────────────────┐
│ 0x40000000: KERNEL CODE/DATA     │  ← Only accessible in EL1
│ (Device tree, kernel, drivers)   │
├──────────────────────────────────┤
│ 0x40100000: USER TASK STACK      │  ← Accessible in EL0
│ 0x40101000: USER TASK HEAP       │     (owns this memory)
│ 0x40102000: USER TASK CODE       │  ← Loaded from kernel
└──────────────────────────────────┘

If user code tries to:
✅ Access own code:         ALLOWED (page permission = user)
✅ Access own heap/stack:   ALLOWED (page permission = user)
✅ Call sys_print:          ALLOWED (svc = legal transition)
❌ Access kernel code:      FAULT (page permission = kernel only)
❌ Disable interrupts:      FAULT (instruction illegal in EL0)
❌ Modify page tables:      FAULT (instruction illegal in EL0)
```

## Key Differences from Phases 1-4

| Feature | Phase 1-4 | Phase 5 |
|---------|-----------|---------|
| **Privilege Levels** | Everything in EL1 | EL0 (user) + EL1 (kernel) |
| **Memory Isolation** | None (all kernel) | User code isolated in memory |
| **Hardware Access** | Direct | Via system calls only |
| **Context Switch** | Thread/Process switch | + privilege level switch |
| **Exception Sources** | Interrupts, timer | + SVC (user syscalls) |
| **Memory Model** | Shared kernel space | Kernel + isolated user spaces |
| **Security** | None (single level) | Process isolation |
| **Crash Isolation** | N/A | User crash ≠ kernel crash |

## Why This Matters

```
Without user mode (Phase 1-4):
┌─────────────────────────┐
│ All code = Kernel      │
├─────────────────────────┤
│ User app crashes  →    │
│ Whole OS crashes  →    │
│ System down            │
└─────────────────────────┘

With user mode (Phase 5+):
┌─────────────────────────┐
│ Kernel = Protected     │
├─────────────────────────┤
│ User app crashes  →    │
│ Just app terminated→   │
│ System continues       │
└─────────────────────────┘
```

## What You'll See in This Phase

1. **Boot in kernel mode (EL1)**
   - Set up exception vectors
   - Create user task in memory
   - Switch to user mode
   
2. **User task running (EL0)**
   - Executes user code
   - Calls sys_print via SVC
   - Calls sys_exit via SVC
   
3. **Kernel interrupt handling**
   - Catches SVC exception
   - Validates syscall
   - Executes kernel code
   - Returns to user mode
   
4. **User task exits**
   - Kernel detects sys_exit
   - Cleans up user task
   - System idle or schedules next task

---

*Understanding privilege levels is KEY to real operating systems. This foundation enables security, fault isolation, and controlled hardware access.*
