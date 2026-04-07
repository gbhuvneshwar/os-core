# MY KERNEL - PHASE 4 THREADING

**Multiple threads executing within a single shared process!**

## Overview

This kernel demonstrates **threading** - a lighter-weight form of concurrency compared to processes. Instead of multiple independent processes with separate memory spaces, this system runs multiple **threads** that share the same kernel memory (code, data, heap) but maintain separate stacks and register states.

## What Are Threads?

### Threads vs Processes

**Process (Phase 4a):**
- Own memory space (code, data, heap, stack)
- Heavy context switching (page tables, TLB flush)
- Slower to create
- Good isolation

**Thread (this phase):**
- Share memory space (code, data, heap)
- Own stack (4KB each)
- Light context switching (registers only)
- Fast to create
- Direct communication via shared memory

## Key Features

✅ **3 Concurrent Threads** (A, B, C) in one process
✅ **Shared Memory** - all threads access same heap/code/data
✅ **Separate Stacks** - each thread has 4KB private stack
✅ **Round-Robin Scheduler** - fair CPU distribution
✅ **Thread Control Block (TCB)** - per-thread metadata
✅ **Fast Context Switch** - only registers change

## How It Works

### Architecture
```
ONE PROCESS
├─ Code (shared by all threads)
├─ Data (shared by all threads)
├─ Heap (shared by all threads)
└─ Three Separate Stacks:
   ├─ Thread A Stack (4KB)
   ├─ Thread B Stack (4KB)
   └─ Thread C Stack (4KB)
```

### Execution
```
Thread A:
  1. Runs thread_a() function
  2. Has 4KB private stack
  3. Accesses shared heap/code/data
  4. Prints "AAA..."
  5. Returns (yields CPU)

Thread B:
  1. Runs thread_b() function
  2. Has different 4KB stack
  3. Accesses SAME heap/code/data as A
  4. Prints "BBB..."
  5. Returns (yields CPU)

Thread C:
  1. Runs thread_c() function
  2. Has different 4KB stack
  3. Accesses SAME heap/code/data
  4. Prints "CCC..."
  5. Returns (yields CPU)

Scheduler cycles: A → B → C → A → B → C...
```

## Output Example

```
--- Thread Cycle ---
[Thread A] Started in shared process!
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

[Thread B] Started in shared process!
BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB

[Thread C] Started in shared process!
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC

--- Thread Cycle ---
[runs again...]
```

All three threads visible, cycling indefinitely!

## File Structure

```
my_kernel_phase_4_thread/
├─ boot.S          (ARM64 bootloader)
├─ kernel.ld       (Linker script)
├─ Makefile        (Build rules)
├─ kernel.c        (Thread functions, main)
├─ thread.h        (TCB, thread API)
├─ thread.c        (Thread scheduler)
├─ uart.c/h        (Serial output)
├─ memory.c/h      (Memory management)
├─ exceptions.c/h  (Exception handling)
├─ THREADING_EXPLAINED.md
└─ THREADING_IMPLEMENTATION.md
```

## Implementation Details

### Thread Creation
```c
create_thread(thread_a, "Thread_A"):
  1. Allocate 4KB stack from physical memory
  2. Create TCB (Thread Control Block)
  3. Initialize entry_point to thread_a function
  4. Set state to READY
  5. Return thread ID
```

### Thread Scheduler
```c
schedule_next_thread():
  1. Get current thread
  2. Increment time used counter
  3. If time slice expired (50ms):
    a. Find next READY thread
    b. Save current thread's SP
    c. Load next thread's SP
    d. Jump to next thread
```

### Thread Control Block
```c
typedef struct {
    u32 tid;                      // Thread ID (0-7)
    u32 state;                    // READY/RUNNING/BLOCKED/DEAD
    u64 stack_pointer;            // Current stack position
    u64 stack_base;               // Stack start address
    u64 stack_size;               // 4KB per thread
    void (*entry_point)(void);   // Thread function
    char name[32];                // "Thread_A", etc
} tcb_t;
```

## Building & Running

### Compile
```bash
cd my_kernel_phase_4_thread
make clean    # Clean previous builds
make all      # Build kernel8.img
```

**Output:**
```
=== BUILD SUCCESS ===
start address 0x0000000040000000
kernel8.img (15,023 bytes)
```

### Run in QEMU
```bash
make run
```

**Emulation:**
- ARM64 Cortex-A72 CPU
- 128MB RAM
- QEMU virt machine

### Expected Output
```
=== THREADS RUNNING IN SHARED PROCESS ===
Watch A, B, C - they execute in parallel!
All threads share the same memory space.

--- Thread Cycle ---
[Thread A] Started in shared process!
AAAA... (80 A's)
[Thread B] Started in shared process!
BBBB... (80 B's)
[Thread C] Started in shared process!
CCCC... (80 C's)
[repeats continuously]
```

## Memory Layout

```
0x40000000 ─────────────────
           Kernel Code/BSS
0x40020000 Exception Vectors
           Page Tables
0x40080000 Heap (shared)
           
0x40088000 Thread A Stack (4KB)
0x40089000 Thread B Stack (4KB)
0x4008A000 Thread C Stack (4KB)
0x4008B000 Free memory
           ...
0x48000000 RAM End (128MB)
```

## Why Threads?

### Advantages
1. **Speed:** 5-10x faster context switch than processes
2. **Memory:** Shared code/data (less memory usage)
3. **Communication:** Direct memory access (no IPC overhead)
4. **Simplicity:** Same kernel code for all threads
5. **Scalability:** Can have many more threads than processes

### Challenges
1. **Race conditions:** Shared memory can cause conflicts
2. **Synchronization:** Need mutexes, semaphores
3. **Complexity:** Concurrency bugs harder to debug
4. **All-or-nothing:** One thread crash can crash kernel

## Comparison: Process vs Thread

| Aspect | Process | Thread |
|--------|---------|--------|
| **Memory Space** | Separate | Shared |
| **Creation** | Slow | Fast |
| **Context Switch** | 100+ cycles | 10-20 cycles |
| **Communication** | Via IPC | Direct access |
| **Isolation** | Good | None |
| **Stacks** | Unique | Separate but in same space |
| **Code** | Duplicated | Single copy |

## Next Steps (Future Phases)

1. **Synchronization Primitives:**
   - Mutexes (locks)
   - Semaphores  
   - Atomic operations

2. **Thread Management:**
   - Thread IDs in registers
   - Thread-local storage
   - Thread cancellation

3. **Preemptive Multitasking:**
   - Timer-driven switches (currently cooperative)
   - No need for threads to yield

4. **Advanced Scheduling:**
   - Priority levels
   - Real-time scheduling
   - CPU affinity

## Testing Verification

- ✅ Kernel compiles with 0 errors
- ✅ Runs in QEMU ARM64
- ✅ Three threads visible in output
- ✅ Cycling between A, B, C continuously
- ✅ All threads output their characters
- ✅ Shared memory (all use same UART)
- ✅ Stability (runs indefinitely)

## Technical Specifications

| Spec | Value |
|------|-------|
| **CPU** | ARM64 Cortex-A72 |
| **RAM** | 128MB (0x40000000-0x48000000) |
| **Threads** | Up to 8 (MAX_THREADS) |
| **Stack/Thread** | 4KB |
| **Time Slice** | 50ms per thread |
| **Scheduler** | Round-robin |
| **Build Size** | 15,023 bytes |

## Code Examples

### Creating a Thread
```c
// In kernel_main()
int tid = create_thread(thread_a, "Thread_A");
if (tid >= 0) {
    uart_puts("Thread created!\n");
}
```

### Thread Function
```c
void thread_a(void) {
    uart_puts("[Thread A] Running\n");
    // Can access shared heap, code, data
    for (int i = 0; i < 80; i++) {
        uart_putc('A');  // Shared UART
    }
    // Return to yield CPU to next thread
}
```

### Getting Current Thread
```c
tcb_t* current = get_current_thread();
uart_puts(current->name);  // "Thread_A"
```

## Documentation Files

- **THREADING_EXPLAINED.md** - Deep dive into threading concepts
- **THREADING_IMPLEMENTATION.md** - Complete implementation details
- **This README.md** - Quick start and overview

## Summary

This phase successfully implements **threading** - a lighter-weight concurrency model where multiple threads share a single process's memory space.

**Key Achievement:** Multiple threads (A, B, C) executing within a shared kernel process, demonstrating:
- Separate stacks per thread
- Shared memory (code, data, heap)
- Light-weight context switching
- Round-robin scheduling

**Status:** ✅ Complete and tested

---

*Threading Kernel | ARM64 | QEMU virt | Phase 4 | 3 Threads in 1 Process*
