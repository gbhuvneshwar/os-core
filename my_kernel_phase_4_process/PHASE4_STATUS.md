# Phase 4 Implementation Status

## PROJECT GOAL
Implement Phase 4 (Processes and Threads) - Round-robin scheduler with two processes (A and B) that time-slice every 50ms.

## WHAT HAS BEEN COMPLETED

### ✅ Architecture & Design
- **PCB Structure**: Process Control Block with all required fields
  - pid, state, context, stack, time_used, entry_point
  - Process states: READY, RUNNING, BLOCKED, DEAD

- **Round-Robin Scheduler**: Complete implementation
  - MAX_PROCESSES = 4
  - TIME_SLICE_MS = 50 (each process gets 50ms before switch)
  - Scheduler selects next READY process every time slice

- **Process Management**:
  - create_process(): Allocates stack, initializes PCB, stores entry_point
  - Two test processes: process_a() and process_b()
  - get_current_process(), get_current_process_id(), get_process_by_id()

- **Memory Management**: From Phase 3
  - 128MB RAM, 4KB pages, bitmap PMM
  - Heap allocator for process structures  
  - Stack allocation per process (4KB per process)

- **Virtual Memory**: 
  - MMU setup with 1GB blocks
  - Page tables configured
  - Identity mapping working

- **Exception Handling**:
  - Exception vector table installed at VBAR_EL1
  - Exception handlers defined (sync, irq, fiq, serror)
  - IRQ handler structure in place

### ✅ Compilation & Build
- Kernel compiles with 0 errors  
- 1 expected warning: RWX segment (acceptable for bare-metal)
- Build product: kernel8.img (14,752 bytes)
- Successfully loads in QEMU ARM64

### ✅ Kernel Execution
- Kernel boots correctly
- All initialization steps complete:
  - ✓ UART working
  - ✓ Memory system initialized (32,632 free pages)
  - ✓ MMU enabled
  - ✓ Scheduler initialized  
  - ✓ Exception vectors installed
  - ✓ Processes created with proper stacks
- Process A launches and runs

### ✅ Documentation
- Created: README.md, QUICKSTART.md, TEST_REPORT.md
- Explained architecture, features, and design decisions

## WHAT IS NOT WORKING

### ❌ Timer Interrupt Delivery
**Problem**: Timer interrupts are not firing
- Timer initialization appears successful 
- VBAR_EL1 correctly set
- Interrupts are enabled (daifclr executed)
- BUT: schedule_next_process() is never called
- Timer dots never appear
- This prevents context switching

**Root Cause**: Likely one of:
1. QEMU ARM64 virtual timer (CNTV_*) not delivering interrupts in this configuration
2. Need to use physical timer (CNTP_*) instead
3. Timer interrupt routing issue in QEMU virt machine
4. EL1 execution mode vs expected configuration

**Evidence Against Interrupts Working**:
- No dots printed (every 1000 timer ticks)
- schedule_next_process() never executes
- Debug counter shows 0 calls to scheduler function

### ❌ Process Switching
**Problem**: Process B never runs
- Process A runs indefinitely
- context_switch_requested flag never set
- Scheduler loop in kernel_main never activates
- Direct cause: Timer interrupts not firing to trigger scheduling

## WHAT WAS ATTEMPTED TO FIX IT

1. **Added context switch detection in process loops**:
   - Processes check `context_switch_requested` flag
   - Should break out when flag is set
   - Code in place but never triggered

2. **Created scheduler main loop in kernel_main**:
   - Replaces direct process_a() call
   - Would properly handle process switching  
   - Waiting for timer to set context_switch_requested

3. **Added debug output**:
   - schedule_next_process() prints "." every 100 calls
   - Timer would print "." every 1000 ticks
   - No debug output seen = timer not firing

## NEXT STEPS

### Immediate Priority  
1. **Debug timer interrupt issue**:
   - Try physical timer (CNTP_TVAL_EL0, CNTP_CTL_EL0) instead of virtual
   - Verify QEMU virt machine supports ARM generic timers at EL1
   - Check if we need to configure interrupt controller (GIC)
   - Verify PSTATE I-bit is actually cleared

2. **Alternative: Use different interrupt source**:
   - QEMU may provide other interrupt mechanisms
   - Check for GIC (ARM Generic Interrupt Controller) support
   - May need to program GIC for timer delivery

### If Timer Works
1. Remove debug output
2. Test process A and B switching  
3. Verify alternating output pattern
4. Complete documentation

## FILES MODIFIED
- `kernel.c`: Added context switch globals, scheduler main loop, process functions
- `scheduler.h`: Added entry_point field to PCB, new getter functions
- `scheduler.c`: Scheduler initialization, process creation, round-robin logic
- `exceptions.c`: Associated with timer setup (not modified in recent work)
- `exceptions.h`: Associated with exception handling (not modified)

## CURRENT TEST OUTPUT
```
Process A prints:
[Process A] Started!
[A] iteration 0-2
[Process A] Entering main loop
AAAA... (repeating)

Expected with fix:
[Process A]... (50ms worth)
[Process B]... (50ms worth)  
[Process A]... (repeats)
[Process B]... (repeats)
```

## ARCHITECTURE DIAGRAM
```
kernel_main()
    ├── uart_init()
    ├── pmm_init() + heap_init()
    ├── mmu_init()
    ├── scheduler_init()
    ├── exceptions_init()
    │   ├── Set VBAR_EL1
    │   ├── timer_init() ← NOT FIRING
    │   └── enable_interrupts()
    ├── create_process(process_a)
    ├── create_process(process_b)
    └── Scheduler loop
        ├── process_a() - runs while context_switch_requested == 0
        └── process_b() - never reached (process_a loops forever)
              ↓ (should happen every 50ms)
         Timer interrupt  ← NOT HAPPENING
              ↓
         exc_irq_handler()
              ↓
         schedule_next_process()  ← NOT CALLED
```

## LESSONS LEARNED
- Timer interrupt delivery is critical path for context switching
- QEMU virtualized timers may require specific configuration
- May need to interact with GIC (ARM Generic Interrupt Controller)
- Testing with simple debug output is essential when interrupts don't fire
