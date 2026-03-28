PHASE 1: BARE METAL BASICS (2-3 weeks)
─────────────────────────────────────────────
  goal: make LED blink OR print "Hello!"
        without ANY operating system!
        just your code + hardware!

  learn:
    ARM64 assembly basics
    linker scripts
    boot process
    UART (serial output)

  tools:
    QEMU (start here!)
    aarch64-none-elf-gcc (cross compiler)
    GDB debugger


PHASE 2: MEMORY MANAGEMENT (3-4 weeks)
─────────────────────────────────────────────
  goal: write your own malloc!
        implement page tables!
        virtual memory working!

  learn:
    physical memory map
    page table setup (ARM64 MMU)
    write your own kmalloc()
    virtual to physical mapping


PHASE 3: EXCEPTIONS AND INTERRUPTS (2-3 weeks)
─────────────────────────────────────────────
  goal: handle timer interrupt!
        handle keyboard input!
        exception vectors!

  learn:
    ARM64 exception levels (EL0, EL1, EL2, EL3)
    interrupt controller (GIC)
    exception vector table
    timer interrupt


PHASE 4: PROCESSES AND THREADS (3-4 weeks)
─────────────────────────────────────────────
  goal: run TWO processes!
        context switch between them!
        YOUR implementation of GIL concept!

  learn:
    process control block (PCB)
    context switch assembly
    scheduler (simple round robin)
    stack per process


PHASE 5: SYSTEM CALLS (2-3 weeks)
─────────────────────────────────────────────
  goal: user space programs
        syscall interface
        privilege levels

  learn:
    ARM64 exception levels
    SVC instruction (syscall)
    user vs kernel mode


PHASE 6: FILESYSTEM (optional, 4+ weeks)
─────────────────────────────────────────────
  goal: read/write files!

  learn:
    SD card driver
    FAT32 filesystem
    VFS (virtual filesystem)