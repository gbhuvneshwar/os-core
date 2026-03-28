#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

/* ═══════════════════════════════════════════════════════════════
   ARM64 EXCEPTION VECTOR TABLE

   Every CPU interrupt/exception jumps to a handler via this table.
   On ARM64, we have 4 exception types:
   - 0: Synchronous (divide by zero, invalid instruction, etc)
   - 1: IRQ (timer, UART, keyboard interrupts)
   - 2: FIQ (fast interrupt - rarely used)
   - 3: SError (system error)

   Each type has 4 vectors (from different privilege levels)
   Total: 16 exception handlers
   ═══════════════════════════════════════════════════════════════ */

/* types */
typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned char  u8;

/* CPU exception types */
#define EXC_SYNC    0  /* Synchronous exception */
#define EXC_IRQ     1  /* Interrupt (IRQ) */
#define EXC_FIQ     2  /* Fast interrupt */
#define EXC_SERROR  3  /* System error */

/* Initialize exception vector table and handlers */
void exceptions_init(void);

/* Enable/disable interrupts at CPU level */
static inline void enable_interrupts(void) {
    /* Clear the I (IRQ disable) bit in PSTATE */
    asm volatile("msr daifclr, #2" ::: "memory");
}

static inline void disable_interrupts(void) {
    /* Set the I (IRQ disable) bit in PSTATE */
    asm volatile("msr daifset, #2" ::: "memory");
}

/* Get current tick count (incremented by timer interrupt) */
u64 get_tick_count(void);

/* Initialize timer to interrupt every 1ms */
void timer_init(void);

#endif
