#include "exceptions.h"
#include "uart.h"
#include "syscall.h"
#include "usermode.h"

/* ═══════════════════════════════════════════════════════════════
   EXCEPTION HANDLING AND TIMER INTERRUPTS

   How it works:
   1. CPU encounters an exception (timer, UART, etc)
   2. CPU reads VBAR_EL1 register (points to exception vector table)
   3. CPU jumps to appropriate handler (based on exception type)
   4. Handler runs, can modify registers/memory
   5. Handler returns with 'eret' instruction
   6. CPU resumes execution at next instruction

   The exception vector table has 16 entries:
   - 4 for synchronous exceptions
   - 4 for IRQ interrupts (this is where timer goes!)
   - 4 for FIQ
   - 4 for SError
   ═══════════════════════════════════════════════════════════════ */

/* Global tick counter - incremented by timer interrupt */
static volatile u64 tick_count = 0;

/* Global timer reload value (ticks per 1ms) */
static volatile u64 timer_ticks_per_ms = 0;

/* Exception vector table - defined in assembly below */
extern void exception_vector_table(void);

/* ═══════════════════════════════════════════════════════════════
   ARM64 EXCEPTION VECTOR TABLE

   Each entry is 128 bytes (32 instructions).
   This is required by ARM64 spec.

   Layout:
   [0x0]   Current EL, SP0 - Synchronous
   [0x80]  Current EL, SP0 - IRQ
   [0x100] Current EL, SP0 - FIQ
   [0x180] Current EL, SP0 - SError
   [0x200] Current EL, SPx - Synchronous
   ... and so on for other exception levels

   We're mostly running at EL1 (kernel mode), so entries at:
   0x200, 0x280, 0x300, 0x380 are important
   ═══════════════════════════════════════════════════════════════ */

/* Exception handlers (will implement below) */
void exc_sync_handler(void);
void exc_svc_handler(void);    /* SVC (System Call) handler - NEW! */
void exc_irq_handler(void);
void exc_fiq_handler(void);
void exc_serror_handler(void);

/* The actual exception vector table in assembly */
asm(
    ".section .text.exceptions\n"
    ".align 11\n"  /* Align to 2KB (2^11 bytes) */
    ".global exception_vector_table\n"
    "exception_vector_table:\n"

    /* ─────────────────────────────────────────────────────
       CURRENT EL with SP0 (EL1 using SP0)
       This is rarely used; included for completeness
       ───────────────────────────────────────────────────── */

    /* 0x000: Synchronous */
    "    brk #1\n"           /* Breakpoint (unimplemented) */
    ".align 7\n"            /* Pad to 128 bytes */

    /* 0x080: IRQ */
    "    brk #1\n"
    ".align 7\n"

    /* 0x100: FIQ */
    "    brk #1\n"
    ".align 7\n"

    /* 0x180: SError */
    "    brk #1\n"
    ".align 7\n"

    /* ─────────────────────────────────────────────────────
       CURRENT EL with SPx (EL1 using SP1) ← WE USE THIS!
       When an exception occurs at EL1, we use SPx (the kernel stack)
       ───────────────────────────────────────────────────── */

    /* 0x200: Synchronous Exception Handler */
    "    str x30, [sp, #-16]!\n"    /* Save return register */
    "    bl exc_sync_handler\n"     /* Call handler */
    "    ldr x30, [sp], #16\n"       /* Restore return register */
    "    eret\n"                     /* Return from exception */
    ".align 7\n"

    /* 0x280: IRQ Handler (TIMER INTERRUPT!) */
    "    str x30, [sp, #-16]!\n"    /* Save return register */
    "    bl exc_irq_handler\n"      /* Call handler */
    "    ldr x30, [sp], #16\n"
    "    eret\n"
    ".align 7\n"

    /* 0x300: FIQ Handler */
    "    str x30, [sp, #-16]!\n"
    "    bl exc_fiq_handler\n"
    "    ldr x30, [sp], #16\n"
    "    eret\n"
    ".align 7\n"

    /* 0x380: SError Handler */
    "    str x30, [sp, #-16]!\n"
    "    bl exc_serror_handler\n"
    "    ldr x30, [sp], #16\n"
    "    eret\n"
    ".align 7\n"

    /* ─────────────────────────────────────────────────────
       LOWER EL (EL0) - User mode exceptions
       When user programs cause exceptions (including syscalls!)
       ───────────────────────────────────────────────────── */

    /* 0x400: Lower EL (AArch64) - Synchronous (includes SVC!) */
    "    str x30, [sp, #-16]!\n"    /* Save return register */
    "    bl exc_svc_handler\n"      /* Call SVC handler */
    "    ldr x30, [sp], #16\n"       /* Restore return register */
    "    eret\n"                     /* Return to user mode */
    ".align 7\n"

    /* 0x480: Lower EL (AArch64) - IRQ */
    "    brk #1\n"
    ".align 7\n"

    /* 0x500: Lower EL (AArch64) - FIQ */
    "    brk #1\n"
    ".align 7\n"

    /* 0x580: Lower EL (AArch64) - SError */
    "    brk #1\n"
    ".align 7\n"

    /* ─────────────────────────────────────────────────────
       LOWER EL (AArch32) - Legacy 32-bit mode
       ───────────────────────────────────────────────────── */

    /* 0x600-0x780: AArch32 exceptions (not used in our 64-bit kernel) */
    "    brk #1\n"
    ".align 7\n"
    "    brk #1\n"
    ".align 7\n"
    "    brk #1\n"
    ".align 7\n"
    "    brk #1\n"
    ".align 7\n"
);

/* ═══════════════════════════════════════════════════════════════
   EXCEPTION HANDLER IMPLEMENTATIONS
   ═══════════════════════════════════════════════════════════════ */

void exc_sync_handler(void) {
    uart_puts("[SYNC_EL1]");
    while(1) { asm volatile("wfi"); }
}

/* ═══════════════════════════════════════════════════════════════
   SVC (SYSTEM CALL) EXCEPTION HANDLER - USER MODE TO KERNEL
   ═══════════════════════════════════════════════════════════════
   
   When user mode code executes "svc #0", it causes an exception
   that brings us here in kernel mode (EL1). We then dispatch
   to the appropriate syscall handler.
   
   User mode context:
   - x0 = syscall number
   - x1-x7 = syscall arguments
   - PC = next instruction after svc
   - SP = user stack pointer
   
   After handling, we return to user mode with:
   - x0 = return value
   - Other registers may be modified
*/
void exc_svc_handler(void) {
    /* Save syscall number from x0 before uart_puts clobbers it */
    u64 syscall_num;
    __asm__("mov %0, x0" : "=r"(syscall_num));
    
    uart_puts("[SYSCALL]\n");
    
    /* Dispatch to syscall handler */
    handle_syscall(syscall_num);
    
    /* Advance PC by 4 bytes */
    __asm__(
        "mrs x1, elr_el1 \n"
        "add x1, x1, #4 \n"
        "msr elr_el1, x1 \n"
    );
}

void exc_fiq_handler(void) {
    uart_puts("⚠ FIQ EXCEPTION (not implemented)\n");
    while(1) { asm volatile("wfi"); }
}

void exc_serror_handler(void) {
    uart_puts("⚠ SERROR EXCEPTION (not implemented)\n");
    while(1) { asm volatile("wfi"); }
}

/* ═══════════════════════════════════════════════════════════════
   IRQ HANDLER - THIS IS WHERE TIMER INTERRUPTS ARRIVE!
   ═══════════════════════════════════════════════════════════════ */

void exc_irq_handler(void) {
    /* Increment tick counter every time timer fires */
    tick_count++;

    /* ARM Generic Timer: Reload timer for next interrupt
       Write new TVAL value to trigger next interrupt in 1ms
       This also clears the current interrupt
     */
    asm volatile("msr cntv_tval_el0, %0" : : "r"(timer_ticks_per_ms));

    /* Print a dot every 1000 ticks (≈ every 1 second) to show timer is working */
    if (tick_count % 1000 == 0) {
        uart_puts(".");
    }
}

/* ═══════════════════════════════════════════════════════════════
   EXCEPTION INITIALIZATION
   ═══════════════════════════════════════════════════════════════ */

void exceptions_init(void) {
    uart_puts("=== EXCEPTIONS & INTERRUPTS ===\n");

    /* Step 1: Install exception vector table
       VBAR_EL1 = pointer to exception vector table
       CPU reads this register when an exception occurs
     */
    uart_puts("Installing exception vector table...\n");
    u64 vbar_addr = (u64)&exception_vector_table;
    uart_puts("  Exception vector table at: ");
    uart_puthex(vbar_addr);
    uart_puts("\n");
    asm volatile("msr vbar_el1, %0" : : "r"(vbar_addr));

    /* Verify it was set */
    u64 vbar_check;
    asm volatile("mrs %0, vbar_el1" : "=r"(vbar_check));
    uart_puts("  VBAR_EL1 register value: ");
    uart_puthex(vbar_check);
    uart_puts("\n");

    /* Step 2: Initialize the ARM generic timer */
    uart_puts("Initializing timer...\n");
    timer_init();

    /* Step 3: Enable interrupts at CPU level
       Before this, ALL interrupts are disabled by default.
       Now timer interrupts will reach our handler.
     */
    uart_puts("Enabling interrupts...\n");
    enable_interrupts();

    uart_puts("Interrupts enabled! Timer running.\n");
    uart_puts("You should see dots appearing every second...\n\n");
}

/* ═══════════════════════════════════════════════════════════════
   ARM GENERIC TIMER INITIALIZATION
   ═══════════════════════════════════════════════════════════════ */

void timer_init(void) {
    /* The ARM generic timer provides:
       - CNTFRQ_EL0: Clock frequency (e.g., 62.5 MHz on QEMU)
       - CNTV_TVAL_EL0: Timer compare value
       - CNTV_CTL_EL0: Timer control register

       How it works:
       1. Each clock cycle, CNTVCT_EL0 increments by 1
       2. When CNTVCT_EL0 reaches CNTV_TVAL_EL0, interrupt fires
       3. We set CNTV_TVAL_EL0 to cause interrupt every 1ms

       If frequency is 62.5 MHz, then:
       1 ms = 62,500 clock cycles
    */

    /* Read timer frequency */
    u64 freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    uart_puts("Timer frequency: ");
    uart_putdec(freq);
    uart_puts(" Hz\n");

    /* Calculate ticks for 1ms */
    u64 ticks_per_ms = freq / 1000;
    uart_puts("Ticks per 1ms: ");
    uart_putdec(ticks_per_ms);
    uart_puts("\n");

    /* Save globally so IRQ handler can use it */
    timer_ticks_per_ms = ticks_per_ms;

    /* Set timer to interrupt every 1ms
       CNTV_TVAL_EL0 = initial countdown value
       When this counts down to 0, interrupt fires and reloads automatically
    */
    asm volatile("msr cntv_tval_el0, %0" : : "r"(ticks_per_ms));

    /* Enable timer interrupt:
       CNTV_CTL_EL0 bit 0 = 1 means "timer enabled"
       CNTV_CTL_EL0 bit 1 = 0 means "interrupt not masked"
     */
    u32 ctl = 1;  /* Enable timer */
    asm volatile("msr cntv_ctl_el0, %0" : : "r"(ctl));

    uart_puts("Timer initialized for 1ms interrupts\n");
}

/* Return current tick count */
u64 get_tick_count(void) {
    return tick_count;
}

/* ═══════════════════════════════════════════════════════════════
   SYSCALL DISPATCHER - Routes syscalls to handlers
   ═══════════════════════════════════════════════════════════════ */

void handle_syscall(u64 syscall_number) {
    u64 arg1, arg2, arg3;
    
    /* Get syscall arguments from registers */
    asm volatile("mov %0, x1" : "=r"(arg1));
    asm volatile("mov %0, x2" : "=r"(arg2));
    asm volatile("mov %0, x3" : "=r"(arg3));
    
    switch(syscall_number) {
        case SYS_PRINT: {
            /* Simple print - no debug output to reduce spam */
            char *str = (char *)arg1;
            u32 len = (u32)arg2;
            
            /* Validate pointer is in user memory range */
            if ((u64)str < 0x40100000 || (u64)str > 0x41000000 || len == 0 || len > 256) {
                break;
            }
            
            /* Print safe length */
            for (u32 i = 0; i < len && i < 256; i++) {
                if (str[i] == '\0') break;
                uart_putc(str[i]);
            }
            
            /* Return success in x0 */
            u64 ret = 0;
            asm volatile("mov x0, %0" : : "r"(ret));
            break;
        }
        
        case SYS_EXIT: {
            /* Kill the current user task */
            user_task_t *task = get_current_user_task();
            if (task) {
                kill_user_task(task);
            }
            
            /* Return control to kernel */
            u64 ret = 0;
            asm volatile("mov x0, %0" : : "r"(ret));
            break;
        }
        
        case SYS_GET_COUNTER: {
            u64 counter = get_tick_count();
            asm volatile("mov x0, %0" : : "r"(counter));
            break;
        }
        
        case SYS_SLEEP: {
            u64 ms = arg1;
            u64 start = get_tick_count();
            while ((get_tick_count() - start) < ms) {
            }
            u64 ret = 0;
            asm volatile("mov x0, %0" : : "r"(ret));
            break;
        }
        
        case SYS_READ_CHAR: {
            char c = uart_getc_nonblocking();
            u64 val = (c != 0) ? c : 0;
            asm volatile("mov x0, %0" : : "r"(val));
            break;
        }
        
        default:
            u64 ret = -1;
            asm volatile("mov x0, %0" : : "r"(ret));
    }
}
