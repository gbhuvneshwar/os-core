# Phase 5: System Call Flow Deep Dive

## One Syscall in Extreme Detail

This document traces ONE system call from start to finish, showing exactly what happens in each register and memory location.

---

## System Call: User → Kernel → User

### Before Syscall: User Code Ready

```
Memory at 0x40100000 (Task A code page):
  0x40100000: 0xD4000001    ← SVC #0 (syscall instruction)
  0x40100004: 0x14000000    ← b . (infinite loop)
  0x40100008: 0x14000000    ← b . (infinite loop)
  ...

User Task A State:
  PC = 0x40100000 (CPU will fetch instruction from here)
  SP = 0x40101FF0 (user stack pointer)
  EL = 0 (user mode)
  
Kernel Awareness (TCB):
  VBAR_EL1 = 0x40081000 (exception vector base)
  user_tasks[0].pc = 0x40100000
  user_tasks[0].sp = 0x40101FF0
  user_tasks[0].state = TASK_RUNNING
```

---

## Step 1: CPU Fetches Instruction

```
Cycle 1: Instruction Fetch
  ├─ CPU reads PC = 0x40100000
  ├─ CPU fetches 32-bit word: 0xD4000001
  ├─ CPU decodes: "SVC #0"
  │  ├─ Instruction: Supervisor Call
  │  ├─ Argument: immediate value 0 (ignored in practice)
  │  └─ Effect: Raise synchronous exception
  └─ CPU recognizes this requires privilege level change
```

---

## Step 2: CPU Raises Exception

```
CPU's Hardware Exception Logic:
  ├─ Current EL = 0 (user mode)
  ├─ Instruction = SVC (requires EL1)
  ├─ Decision: RAISE EXCEPTION
  │
  └─ Exception Entry (CPU does this automatically):
      ├─ Read VBAR_EL1 = 0x40081000
      ├─ Determine exception type:
      │  ├─ PC was in EL0 (Lower EL)
      │  └─ Exception type: Synchronous (SVC)
      │  └─ Vector offset = 0x400 (Lower EL Sync = bits [5:3]=0, bits [8:6]=4)
      │
      ├─ Jump to VBAR_EL1 + 0x400 = 0x40081000 + 0x400 = 0x40081400
      │
      ├─ SAVE User Context (CPU does this automatically):
      │  ├─ ELR_EL1 = 0x40100000 (store user PC for later return)
      │  ├─ SPSR_EL1 = 0 (store user privilege level for later return)
      │  └─ SP = kernel stack is restored (prepare for kernel code)
      │
      └─ Privilege Level Changes: EL0 → EL1
         (User mode → Kernel mode)
```

---

## Step 3: Exception Handler Executes (Assembly at 0x40081400)

```
Handler Location: 0x40081400 (in kernel memory)

Assembly Code:
┌─────────────────────────────────────┐
│ 0x40081400:  str x30, [sp, #-16]!   │
└─────────────────────────────────────┘
  Operation:
    ├─ sp <- sp - 16 (adjust stack pointer down by 16 bytes)
    ├─ memory[sp] <- x30 (push x30 onto kernel stack)
    │
    └─ Why? Save x30 (link register) for nested function calls
      
      Before: SP = 0x40000000 (kernel stack top)
      After:  SP = 0x39FFFFF0 (16 bytes lower)
      Memory: [0x39FFFFF0] = old x30 value

┌─────────────────────────────────────┐
│ 0x40081408:  bl exc_svc_handler     │
└─────────────────────────────────────┘
  Operation:
    ├─ x30 = 0x40081408 + 4 = 0x4008140C (save return address)
    ├─ PC = exc_svc_handler (function pointer, jumps to C code)
    │
    └─ Now we're in C code (exc_svc_handler function)

┌─────────────────────────────────────┐
│ exc_svc_handler() [C code]          │
└─────────────────────────────────────┘
  (See next section - detailed explanation)

┌─────────────────────────────────────┐
│ 0x40081410:  ldr x30, [sp], #16     │
└─────────────────────────────────────┘
  Operation:
    ├─ x30 = memory[sp] (pop x30 from kernel stack)
    ├─ sp <- sp + 16 (adjust stack pointer up by 16 bytes)
    │
    └─ Why? Restore x30 before returning

      Before: SP = 0x39FFFFF0, memory[SP] = saved x30
      After:  SP = 0x40000000, x30 = restored value

┌─────────────────────────────────────┐
│ 0x40081414:  eret                   │
└─────────────────────────────────────┘
  THE CRITICAL INSTRUCTION!
  
  ERET = Exception Return
  
  CPU Hardware Action:
    ├─ Read ELR_EL1 (Exception Link Register)
    │  └─ ELR_EL1 = 0x40100004 (updated by exc_svc_handler!)
    │
    ├─ Read SPSR_EL1[3:0] (Saved Processor State bits [3:0])
    │  └─ SPSR_EL1[3:0] = 0 (means EL0)
    │
    ├─ Privilege Level Changes: EL1 → EL0 (back to user mode)
    │
    ├─ PC = ELR_EL1 = 0x40100004
    │  └─ CPU jumps to NEXT instruction (0x40100004, not SVC!)
    │
    └─ Execution continues in EL0 at 0x40100004
```

---

## Step 4: C Handler Function Details

```c
void exc_svc_handler(void) {
    // ══════════════════════════════════════════════════════
    // PART 1: GET SYSCALL NUMBER FROM USER
    // ══════════════════════════════════════════════════════
    
    u64 syscall_num;
    
    __asm__("mov %0, x0" : "=r"(syscall_num));
    //        ^^^            ^^^
    //        Read x0 reg    Store in syscall_num variable
    //
    // Inline assembly:
    //   "mov %0, x0"       - ARM64 instruction: move x0 to output
    //   : "=r"(syscall_num) - Output constraint: write-only register
    //
    // What this does:
    //   - User code put syscall number in x0 before SVC
    //   - We read x0 and store in local variable
    //   - For our test: syscall_num = 0

    // ══════════════════════════════════════════════════════
    // PART 2: PRINT DEBUG MESSAGE
    // ══════════════════════════════════════════════════════
    
    uart_puts("[SYSCALL]\n");
    //
    // Output goes to serial port (UART)
    // This proves we reached kernel mode from user code!
    // What user sees in terminal: "[SYSCALL]"

    // ══════════════════════════════════════════════════════
    // PART 3: DISPATCH TO SYSCALL HANDLER
    // ══════════════════════════════════════════════════════
    
    handle_syscall(syscall_num);
    //
    // Call syscall dispatcher:
    //
    // void handle_syscall(u64 syscall_num) {
    //     switch (syscall_num) {
    //         case 0x0:  break;  // Null syscall
    //         case 1:    // SYS_PRINT
    //         case 2:    // SYS_EXIT
    //         case 3:    // SYS_GET_COUNTER
    //         case 4:    // SYS_SLEEP
    //         case 5:    // SYS_READ_CHAR
    //         default:   // Unknown
    //     }
    // }
    //
    // For our test: syscall_num = 0, so nothing happens (just break)

    // ══════════════════════════════════════════════════════
    // PART 4: THIS IS THE MAGIC - ADVANCE PC!
    // ══════════════════════════════════════════════════════
    // 
    // WITHOUT PC advancement:
    //   ERET would jump back to 0x40100000 (SVC instruction)
    //   → CPU executes SVC again
    //   → Exception raised again
    //   → Handler called again
    //   → ERET jumps back to SVC again
    //   → INFINITE LOOP OF EXCEPTIONS! 💥
    //
    // WITH PC advancement:
    //   ERET jumps to 0x40100004 (next instruction, endless loop)
    //   → User code continues
    //   → No more exceptions
    //   → Stable execution ✅
    
    __asm__(
        "mrs x1, elr_el1 \n"    // Step A: Read ELR_EL1
        "add x1, x1, #4 \n"     // Step B: Add 4 (instruction size)
        "msr elr_el1, x1 \n"    // Step C: Write back to ELR_EL1
    );
    
    // Step-by-step:
    // ┌──────────────────────────────────────────────────────┐
    // │ "mrs x1, elr_el1"                                    │
    // ├────────────────────────────────────────────────────── │
    // │ MRS = "Move from (system) Register to register"      │
    // │ Read the Exception Link Register (ELR_EL1)          │
    // │ Store in x1                                          │
    // │                                                      │
    // │ Before: ELR_EL1 = 0x40100000 (address of SVC)      │
    // │ After:  x1 = 0x40100000                            │
    // └──────────────────────────────────────────────────────┘
    
    // ┌──────────────────────────────────────────────────────┐
    // │ "add x1, x1, #4"                                     │
    // ├────────────────────────────────────────────────────── │
    // │ ADD = Add 4 to x1                                    │
    // │                                                      │
    // │ Why #4?                                             │
    // │   ARM64 instructions are always 4 bytes long         │
    // │   SVC #0 at 0x40100000 is 4 bytes                   │
    // │   Next instruction is at 0x40100000 + 4 = 0x40100004 │
    // │                                                      │
    // │ Before: x1 = 0x40100000                            │
    // │ After:  x1 = 0x40100004                            │
    // └──────────────────────────────────────────────────────┘
    
    // ┌──────────────────────────────────────────────────────┐
    // │ "msr elr_el1, x1"                                    │
    // ├────────────────────────────────────────────────────── │
    // │ MSR = "Move (from register) to System Register"      │
    // │ Write x1 back to Exception Link Register            │
    // │                                                      │
    // │ Before: ELR_EL1 = 0x40100000 (old value)           │
    // │ After:  ELR_EL1 = 0x40100004 (new value!)          │
    // │                                                      │
    // │ Result: When ERET executes, PC will be set to       │
    // │         0x40100004 (NEXT instruction, not SVC!)     │
    // └──────────────────────────────────────────────────────┘
    
    // Now when ERET happens (in assembly below):
    // CPU reads ELR_EL1 = 0x40100004
    // CPU jumps there instead of 0x40100000
    // NO INFINITE LOOP!
}
```

---

## Step 5: Return to User Mode

```
Assembly resumes at line after exc_svc_handler():
┌─────────────────────────────────────┐
│ 0x40081410:  ldr x30, [sp], #16     │
└─────────────────────────────────────┘
  (Restore return register - done)

┌─────────────────────────────────────┐
│ 0x40081414:  eret                   │
└─────────────────────────────────────┘
  
  ERET Execution (CPU hardware does this):
  
  ├─ Step 1: Read ELR_EL1
  │  └─ ELR_EL1 = 0x40100004 (thanks to exc_svc_handler!)
  │
  ├─ Step 2: Read SPSR_EL1[3:0]
  │  └─ SPSR_EL1[3:0] = 0 (EL0 = user mode)
  │
  ├─ Step 3: Restore Privilege Level
  │  └─ Current EL = 0 (EL1 → EL0 transition)
  │
  ├─ Step 4: Restore Instruction Pointer
  │  └─ PC = 0x40100004
  │
  └─ Result: Back in user mode, at next instruction!
```

---

## Step 6: User Code Continues

```
User Code is Now at: 0x40100004

Memory at 0x40100004:
  0x40100004: 0x14000000  ← This is "b . " (branch to self)
  
CPU execution at EL0:
  ├─ Fetch instruction at 0x40100004
  ├─ Decode: b . (branch instruction)
  ├─ Execute: Branch to address 0x40100004 (itself)
  ├─ Next instruction: 0x40100004 (same location!)
  │
  └─ Result: Infinite loop
      ├─ Keeps executing b . at 0x40100004 forever
      ├─ No more exceptions
      ├─ No kernel involvement
      ├─ User code happy in user mode
      └─ System completely stable! ✅
```

---

## Register State Throughout Syscall

### Timeline of Register Changes

```
TIME    Location            EL    PC              ELR_EL1         SPSR_EL1[3:0]
────────────────────────────────────────────────────────────────────────────────
1ms    0x40100000          0     0x40100000      (unchanged)      (unchanged)
       User about to SVC

2ms    0x40100000          0     0x40100000      (unchanged)      (unchanged)
       CPU decoding SVC

3ms    0x40081400 ←EXCEPTION    0x40081400      0x40100000       0
       Kernel handler       1                    (saved)

4ms    0x40082xxx          1     0x40082xxx      0x40100000       0
       In exc_svc_handler()                     (unchanged)

5ms    exc_svc_handler()   1     exc_svc_...    0x40100000       0
       Reading ELR_EL1                          (unchanged)

6ms    exc_svc_handler()   1     exc_svc_...    0x40100004       0
       After PC = +4        (MODIFIED!)

7ms    0x40081414          1     0x40081414     0x40100004       0
       About to ERET

8ms    0x40100004 ←ERET!   0     0x40100004     0x40100004       0
       Back in user        (EL1 → EL0)

9ms    0x40100004          0     0x40100004     0x40100004       0
       Infinite loop       (no more changes)
```

---

## Data Flow: What Goes Where

### Register Argument Passing

In ARM64, function arguments are passed in registers x0-x7:

```
User Syscall Example (SYS_PRINT):
┌─────────────────────────────────────┐
│ USER CODE (before SVC)              │
├─────────────────────────────────────┤
│ mov x0, #1           ; SYS_PRINT    │
│ mov x1, #msg_ptr     ; pointer      │
│ mov x2, #msg_len     ; length       │
│ svc #0               ; SYSCALL      │
└─────────────────────────────────────┘
         ↓ (exception)
┌─────────────────────────────────────┐
│ KERNEL HANDLER                      │
├─────────────────────────────────────┤
│ exc_svc_handler() {                 │
│   u64 syscall_num;                  │
│   __asm__("mov %0, x0" :            │
│       "=r"(syscall_num));          │
│                                     │
│   // syscall_num = 1 (SYS_PRINT)   │
│   // x1 still has msg_ptr           │
│   // x2 still has msg_len           │
│                                     │
│   handle_syscall(syscall_num);     │
│   // ... can read x1, x2 from       │
│   // user context if needed         │
│ }                                   │
└─────────────────────────────────────┘
         ↓ (ERET)
┌─────────────────────────────────────┐
│ USER CODE (gets return value)       │
├─────────────────────────────────────┤
│ // x0 now contains return value     │
│ // (0 for success, usually)         │
│                                     │
│ mov x3, x0           ; save result  │
│ b .                  ; loop         │
└─────────────────────────────────────┘
```

---

## Why This Matters

### Security Implications

```
Threat Model Analysis:

┌────────────────────────────────────────────────────────┐
│ Can user code modify SPSR_EL1 to become kernel?       │
├────────────────────────────────────────────────────────┤
│ NO! ✅                                                 │
│ If user tries: msr spsr_el1, x0                       │
│ CPU says: "NO! You're in EL0, privileged instruction" │
│ Exception raised (privilege violation)                │
│ Kernel exception handler catches it                   │
│ Kernel terminates user task                          │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│ Can user code access kernel memory (0x40000000)?      │
├────────────────────────────────────────────────────────┤
│ NO! ✅ (with MMU - Phase 6)                           │
│ If user tries: ldr x0, [0x40000000]                   │
│ CPU checks: Current EL = 0 access to KERNEL memory?  │
│ Exception raised (memory protection violation)        │
│ Kernel exception handler catches it                   │
│ Kernel terminates user task                          │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│ Can user code prevent kernel from running?            │
├────────────────────────────────────────────────────────┤
│ NO! ✅                                                 │
│ User code has no way to disable interrupts or         │
│ control exception handlers                           │
│ Kernel always has control via exceptions             │
└────────────────────────────────────────────────────────┘
```

### Real-World Examples

```
This exact mechanism is used by:

Linux (x86_64):
  - User mode: Ring 3
  - Kernel mode: Ring 0
  - Syscall: INT 0x80 or SYSCALL instruction
  - Returns: IRET or SYSRET instruction

Windows (x64):
  - User mode: Ring 3
  - Kernel mode: Ring 0
  - Syscall: SYSCALL instruction
  - Returns: SYSRET instruction

macOS (ARM64):
  - User mode: EL0
  - Kernel mode: EL1
  - Syscall: SVC instruction
  - Returns: ERET instruction ← Same as Phase 5!

Android (ARM64):
  - User mode: EL0
  - Kernel mode: EL1
  - Uses same ARM64 EL0/EL1 architecture

Our Phase 5:
  - User mode: EL0 ✅
  - Kernel mode: EL1 ✅
  - Syscall: SVC instruction ✅
  - Returns: ERET instruction ✅
```

---

## Summary: One Syscall Complete

### What Happened

1. **User Code:** Executed SVC #0 instruction
2. **CPU Hardware:** Detected privilege violation → exception
3. **Exception Handler:** Jumped to 0x40081400
4. **Kernel Code:** Processed syscall, advanced PC
5. **Exception Return:** ERET transitioned back to EL0
6. **User Code:** Continued at next instruction (0x40100004)

### Key Insight

The CPU enforces privilege levels in hardware. There's no way for user code to cheat, trick, or bypass the kernel. The kernel is always in control because:

- User code cannot modify SPSR_EL1 (privileged instruction)
- User code cannot execute ERET (privileged instruction)
- User code cannot modify VBAR_EL1 (privileged instruction)
- User code cannot access kernel memory (CPU enforces)

Only by raising an exception via SVC can user code communicate with the kernel. The kernel processes the request and uses ERET to return. Perfect security model!

This is why privilege levels exist in all modern CPUs. It's the foundation of operating system security.

