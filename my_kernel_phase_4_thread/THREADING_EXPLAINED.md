# Threading Implementation - How It Works

## THREADS vs PROCESSES

### Processes (Phase 4 Process)
```
Memory Layout:
┌─────────────────────────┐
│  Process A:             │
│  ├─ Code segment        │
│  ├─ Data segment        │
│  ├─ Heap (own)          │
│  └─ Stack (own)         │
│  PCB: {memory_map...}   │
└─────────────────────────┘

┌─────────────────────────┐
│  Process B:             │
│  ├─ Code segment        │
│  ├─ Data segment        │
│  ├─ Heap (own)          │
│  └─ Stack (own)         │
│  PCB: {memory_map...}   │
└─────────────────────────┘

Characteristics:
- Isolated memory spaces
- Heavy context switch (flush caches, change page tables)
- IPC required for communication
```

### Threads (this Phase)
```
Memory Layout (Single Process with Multiple Threads):
┌──────────────────────────────────────┐
│         SINGLE PROCESS               │
│  ├─ Code segment (shared)            │
│  ├─ Data segment (shared)            │
│  ├─ Heap (shared)                    │
│  │                                   │
│  ├─ Thread 1 Stack (separate)        │
│  │  └─ TCB: {sp, pc, tid...}         │
│  │                                   │
│  ├─ Thread 2 Stack (separate)        │
│  │  └─ TCB: {sp, pc, tid...}         │
│  │                                   │
│  └─ Thread 3 Stack (separate)        │
│     └─ TCB: {sp, pc, tid...}         │
│                                      │
└──────────────────────────────────────┘

Characteristics:
- Shared memory spaces (code, data, heap)
- Light context switch (only registers/stack pointer)
- Direct memory communication
- Faster creation and switching
```

## KEY DIFFERENCES

| Aspect | Process | Thread |
|--------|---------|--------|
| Memory Space | Own (private) | Shared (global) |
| Creation Time | Slow (allocate all) | Fast (allocate stack only) |
| Context Switch | Slow (flush TLB) | Fast (save registers) |
| Communication | IPC mechanisms | Direct memory access |
| Stack | Isolated | Separate but in same space |
| Code Memory | Separate copies | Single shared copy |
| Data Loss | On exit, data gone | Data persists (thread exits) |

## IMPLEMENTATION DETAILS

### Thread Control Block (TCB)
```c
typedef struct {
    u32 tid;                    // Thread ID (0-7 per process)
    u32 state;                  // READY, RUNNING, BLOCKED
    u64 stack_pointer;          // Current stack position
    u64 stack_base;             // Bottom of stack
    u64 stack_size;             // Stack size (typically 4KB)
    u64 program_counter;        // Where thread is executing
    u64 time_used;              // CPU time counter
    char name[32];              // Debug name
    void (*entry_point)(void); // Thread function
} tcb_t;
```

### Thread Creation Flow
```
create_thread(thread_func, "name"):
  1. Allocate 4KB stack page
  2. Create TCB
  3. Initialize stack pointer to top of stack
  4. Set entry_point to function
  5. Set state to READY
  6. Add to thread queue
  7. Return thread ID
```

### Thread Scheduler
```
schedule_next_thread():
  1. Increment current thread's time counter
  2. Check if time slice expired (50ms)
  3. Find next READY thread
  4. Save current thread's stack pointer
  5. Load next thread's stack pointer
  6. Get next thread's registers (simulated)
  7. Jump to next thread's code
```

### Context Switch Between Threads
```
Before:                         After:
Thread A Running:              Thread B Running:
├─ SP (Stack Pointer) = 0x4008A000
├─ PC (Program Counter) = address in thread_a_code
└─ R0-R30 = ... values ...

                      ↓ (SWITCH)

                      └─ SP = 0x4008B000
                      └─ PC = address in thread_b_code
                      └─ R0-R30 = ... values ...
```

## MEMORY LAYOUT IN THIS IMPLEMENTATION

```
0x40000000 ─────────────────── RAM Start
           ├─ Kernel Code
           ├─ Exception Vectors
           ├─ MMU Page Tables
0x40020000 ├─ Scheduler Data
           ├─ Global Variables
0x40080000 ├─ Heap Start
           │  (allocated for thread stacks)
           │
0x40088000 ├─ Thread 0 Stack (4KB)
           │  0x40088000-0x40089000
0x40089000 ├─ Thread 1 Stack (4KB)
           │  0x40089000-0x4008A000
0x4008A000 ├─ Thread 2 Stack (4KB)
           │  0x4008A000-0x4008B000
0x4008B000 ├─ Thread 3 Stack (4KB)
           │
           ├─ ... more threads ...
           │
0x48000000 ─────────────────── RAM End (128MB)
```

## HOW THREADS EXECUTE

### Example: 2 Threads doing work
```
Time →

Thread 0: Work - Work - Work - [switch] - Work - Work - Work - [switch]
Thread 1:                     - Work - Work - Work - [switch] - Work - Work

Output:  AAA BBB AAA BBB AAA ...
```

### Scheduler Loop
```c
while(1) {
    for each thread in threads {
        if (thread.state == READY) {
            run_thread(thread);      // Execute for time slice
        }
    }
}
```

## WHY THREADS ARE LIGHTER

### Process Context Switch (Heavy)
- Save all CPU registers
- Save/restore memory page tables
- Flush Translation Lookaside Buffer (TLB)
- Change virtual memory mapping
- Time: ~100+ CPU cycles

### Thread Context Switch (Light)
- Save CPU registers (same set!)
- Change stack pointer (SP)
- Change program counter (PC)
- NO page table changes (shared memory)
- NO TLB flush (same memory!)
- Time: ~10-20 CPU cycles

**Result:** Threads are 5-10x faster to switch than processes!

## SHARED MEMORY ISSUES

### Problem: Race Conditions
```c
// Global variable (shared by all threads)
int counter = 0;

// Thread 0 code:
counter++;    // Read counter, increment, write back

// Thread 1 code:
counter++;    // Read counter, increment, write back

// Expected: counter = 2
// Actual: counter might be 1 (race condition!)
// Why: Both threads see initial value 0
```

### Solution: Synchronization (future phases)
- Mutexes (locks)
- Semaphores
- Atomic operations
- Read-write locks

## THIS IMPLEMENTATION

This phase demonstrates:
- ✓ Multiple threads in ONE process
- ✓ Shared memory space
- ✓ Separate stacks per thread
- ✓ Round-robin thread scheduler
- ✓ Thread creation API
- ✓ Thread switching

Not implemented (future phases):
- Synchronization primitives
- Thread-local storage (TLS)
- Condition variables
- Thread cancellation
- Preemptive switching via timer

## TEST SCENARIO

Three threads in one process:
```
Thread 0 (PrintA): Outputs "A" 50 times
Thread 1 (PrintB): Outputs "B" 50 times
Thread 2 (PrintC): Outputs "C" 50 times

All running in parallel:
Output: ABCABCABCABC... or similar interleaving
        (depends on scheduling)
```

## BENEFITS OVER PROCESSES

1. **Speed:** 5-10x faster context switching
2. **Memory:** Shared code/data (less memory)
3. **Communication:** Direct memory sharing (no IPC)
4. **Simplicity:** Easier to implement
5. **Responsiveness:** More threads per system

## SUMMARY

Thread vs Process:
- **Process:** Heavy, isolated, private memory
- **Thread:** Light, shared memory, fast switch

This phase: Multi-threaded single process kernel!
