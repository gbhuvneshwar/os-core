# MyKernel - Bare Metal ARM64 Kernel

A minimal bare metal operating system kernel for ARM64 (AArch64) architecture. This project demonstrates low-level kernel development by booting directly on hardware (or QEMU emulation) without any OS underneath.

## What This Project Does

This is a **bare metal kernel** - code that runs directly on CPU hardware with no operating system, bootloader support, or standard libraries. It:

1. **Boots the CPU** from reset using ARM64 assembly
2. **Sets up memory** (stack initialization and BSS clearing)
3. **Prints messages** to the serial console via UART
4. **Shuts down QEMU** automatically using poweroff registers

Think of it as the absolute minimum code needed to make an ARM64 CPU do something useful - similar to how real OS kernels like Linux or Windows start before loading the rest of the system.

---

## Project Architecture

### Memory Layout (Fixed in QEMU virt)

QEMU's "virt" machine defines fixed memory addresses that never change:

```
Memory Map:
┌─────────────────────────────┐
│   Kernel Code/Data          │
│   0x40000 - 0x50000         │  ← Kernel loaded here by linker
│                             │
│   (free space)              │
│                             │
│   Stack (grows down)        │
│   0x80000 (top of stack)    │  ← Set in boot.S
│                             │
│   Hardware Registers        │
│   (memory-mapped)           │
└─────────────────────────────┘
```

**Why these addresses are fixed:**
- **0x40000** — QEMU virt expects kernel here, defined in hardware spec
- **0x80000** — Stack starts here (grows downward), arbitrary but safe choice
- **Hardware addresses (below)** — These are fixed by the ARM virt board design

---

## Hardware Addresses Explained

These addresses control actual hardware via **memory-mapped I/O** - writing values to specific memory addresses triggers hardware actions.

### UART0_BASE = 0x09000000

**UART = Universal Asynchronous Receiver/Transmitter** (serial port)

```c
#define UART0_BASE   0x09000000
```

**What it does:** Serial communication for console output (printing text)

**Why this address:**
- Fixed by ARM virt board specification (QEMU defines it here)
- This is where the UART hardware controller is accessible in memory
- When you write a byte to this address, the UART sends it out the serial port
- Used for printing kernel messages so we can see what's happening

**In the code:**
```c
void uart_putc(char c) {
    volatile char* uart = (volatile char*)UART0_BASE;
    *uart = c;  /* write character to UART */
}
```

### SYSCON_BASE = 0x09080000

**SYSCON = System Control** (power management controller)

```c
#define SYSCON_BASE        0x09080000
#define SYSCON_POWEROFF    0x5555  /* magic value */
```

**What it does:** Controls system power (shutdown, reboot, etc.)

**Why this address:**
- Fixed by ARM virt board - this is where the power controller lives
- Writing a specific "magic value" (0x5555) tells QEMU to power off
- Real hardware uses similar controllers (though different magic values)

**In the code:**
```c
void qemu_poweroff(void) {
    volatile int* syscon = (volatile int*)SYSCON_BASE;
    *syscon = SYSCON_POWEROFF;  /* write 0x5555 to trigger shutdown */
}
```

**Why are they fixed?**
- Hardware manufacturers define memory layouts in the hardware specification
- QEMU emulates real ARM64 boards and must use their real address layouts
- If you moved to a Raspberry Pi or BeagleBone, addresses would change, but the concept is identical

---

## The volatile Keyword

You'll notice `volatile` before every hardware pointer:

```c
volatile char* uart = (volatile char*)UART0_BASE;
```

**Why it matters:**
- The compiler normally caches values in CPU registers for speed
- But hardware registers **can change without code modifying them**
- `volatile` forces the compiler to re-read from memory each time
- Without it, optimizations would break hardware access

---

## Build & Run Instructions

### Prerequisites

**Option 1: Using Docker (Recommended - works on Mac/Linux/Windows)**

```bash
cd mykernel
docker build -t mykernel .
docker run --rm -it mykernel bash -c "make run"

"""
#docker run -it --rm -v ~/mykernel:/kernel  mykernel-dev bash
#make clean
#make 
#make run
"""

```

**Option 2: Native Setup (Ubuntu/Debian)**

```bash
# Install cross-compiler for ARM64
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-arm make

cd mykernel
make run
```

**Option 3: macOS with Homebrew**

```bash
# Install QEMU and ARM64 cross-compiler
brew install qemu aarch64-elf-gcc

cd mykernel
make run  # May need adjustments for your setup
```

### Build Steps

```bash
# Clean previous builds
make clean

# Build the kernel
make

# Run in QEMU
make run
```

**What happens:**
1. `boot.S` compiles to `boot.o` (ARM64 assembly)
2. `kernel.c` compiles to `kernel.o` (C code)
3. Linker combines them using `kernel.ld` layout → `kernel.elf`
4. `kernel8.img` is the final binary (ELF → raw binary)
5. QEMU loads `kernel8.img` at address 0x40000 and starts executing

---

## Step-by-Step Boot Process

### Stage 1: CPU Reset → boot.S (_start)

```
1. CPU power on → runs code at reset address
2. QEMU jumps to 0x40000 (kernel base)
3. _start (boot.S) executes:
   
   a) Set up Stack Pointer (SP = 0x80000)
      - Stack is used for function calls, local variables
      - Grows DOWN from 0x80000
      
   b) Clear BSS Section
      - BSS = uninitialized global variables
      - Must be zeroed before C code runs
      - Loop: zero 8 bytes at a time until __bss_end
      
   c) Jump to kernel_main()
      - QEMU emulates ARM64, so BL instruction branches with return address
```

**Boot assembly (simplified):**
```asm
_start:
    ldr x30, =0x80000        ; Set return address
    mov sp, x30              ; SP = 0x80000 (stack top)
    
    ldr x0, =__bss_start     ; x0 = start of BSS
    ldr x1, =__bss_end       ; x1 = end of BSS
clear_bss:
    str xzr, [x0], #8        ; Store zero, advance 8 bytes
    bne clear_bss            ; Loop until done
    
    bl kernel_main           ; Call C function
    b hang                    ; If return (shouldn't), loop forever
```

### Stage 2: C Code → kernel_main()

```
4. kernel_main() begins execution (kernel.c)
   
   a) uart_puts() - Print welcome messages
      - Each call walks the string character by character
      - uart_putc() writes to UART0_BASE (0x09000000)
      - Output appears on console/serial port
      
   b) qemu_poweroff() - Graceful shutdown
      - Write 0x5555 to SYSCON (0x09080000)
      - QEMU detects magic value and powers off
      - Returns to hang loop (unreachable)
```

**Console output flow:**
```
uart_puts("Hello!") 
  → uart_putc('H') → *(0x09000000) = 'H' → UART sends 'H'
  → uart_putc('e') → *(0x09000000) = 'e' → UART sends 'e'
  → [continues for each character]
  → Appears on user's terminal
```

### Stage 3: Shutdown

```
5. qemu_poweroff() executes:
   - Write 0x5555 to address 0x09080000
   - QEMU's simulated SYSCON sees magic value
   - QEMU exits gracefully (no hang!)
```

---

## File Descriptions

### `boot.S` - ARM64 Assembly Boot Code
- **Purpose:** CPU startup (first code that runs)
- **What it does:**
  - Set stack pointer (0x80000)
  - Zero uninitialized variables (BSS section)
  - Jump to C code (kernel_main)
- **Why separate from C:** Must run before C runtime is ready

### `kernel.c` - Main Kernel Code
- **uart_putc()** - Write single character to UART
- **uart_puts()** - Write string character-by-character
- **qemu_poweroff()** - Trigger QEMU shutdown
- **kernel_main()** - Entry point from boot.S, main kernel logic

### `kernel.ld` - Linker Script
- **Purpose:** Describe memory layout to the linker
- **Key definitions:**
  - Kernel loads at 0x40000
  - Order: .text (code) → .rodata (const) → .data (initialized vars) → .bss (uninitialized vars)
  - Defines `__bss_start` and `__bss_end` symbols for boot.S

### `Makefile` - Build Instructions
- **Targets:**
  - `make all` - Compile boot.S + kernel.c, link, create kernel8.img
  - `make run` - Build and launch QEMU with kernel
  - `make clean` - Remove build artifacts

### `Dockerfile` - Container for Consistent Build
- Sets up Ubuntu with ARM64 cross-compiler and QEMU
- Ensures same build environment on Mac/Linux/Windows

---

## Key Concepts Explained

### Bare Metal
Running code directly on CPU with:
- ❌ No Operating System
- ❌ No Bootloader
- ❌ No Standard C Library
- ✅ Only your code and QEMU hardware emulation

### Cross-Compilation
Compiling for a different CPU architecture:
- On macOS: `aarch64-linux-gnu-gcc` produces ARM64 code
- Binaries only run on ARM64 (or QEMU emulating ARM64)

### BSS Section
**BSS = Block Started by Symbol** (uninitialized static/global variables)

Must be zeroed before C code runs because:
- C expects globals to start as 0
- RAM on boot contains garbage data
- Boot code explicitly zeros this memory

### linker Script
`kernel.ld` is like a recipe for the linker:
- **WHERE:** Place code at 0x40000
- **ORDER:** Put boot code first (must run before C)
- **EXPORTS:** Define symbols like `__bss_start` for boot.S to use

---

## Expected Output

When you run `make run`:

```
===========================
  MY KERNEL IS RUNNING!   
===========================
Hello from bare metal!
No Linux! No macOS!
Just MY code on ARM64!
===========================
Shutting down QEMU...
```

Then QEMU exits cleanly (no hanging).

---

## Troubleshooting

**"Command not found: aarch64-linux-gnu-gcc"**
- Use Docker or install cross-compiler tools (see Prerequisites)

**"QEMU hangs after printing"**
- Kernel is stuck in the final `while(1) {}` loop
- Check if poweroff() is being called

**"Output looks garbled"**
- Check UART baud rate (matches QEMU's default)
- Ensure serial terminal settings correct

**Different hardware?**
- Change UART0_BASE and SYSCON_BASE addresses
- Check your hardware's memory map documentation
- All other code remains the same!

---

## Next Steps to Extend This Kernel

- **Memory Management:** Implement page tables and virtual memory
- **Interrupts:** Handle timer interrupts and context switching
- **Multi-core:** Boot secondary CPU cores
- **Device Tree:** Parse hardware description instead of hardcoding addresses
- **Filesystem:** Read/write disk using ATA or VIRTIO
- **Scheduler:** Simple round-robin task scheduler

---

## References

- [ARM64 Assembly Reference](https://developer.arm.com/documentation/den0024/)
- [QEMU virt Board Manual](https://www.qemu.org/docs/master/system/arm/virt.html)
- [ARM64 System Control Documentation](https://developer.arm.com/documentation/)
- [OSDev.org - Bare Metal ARM](https://wiki.osdev.org/ARM_Bare_Metal)

---

**Happy Kernel Hacking! 🚀**
