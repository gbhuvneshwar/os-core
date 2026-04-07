/*
 * syscall.h - System Call Interface for user mode programs
 * 
 * Defines the syscall numbers and interface for user->kernel communication
 * Users execute: svc #0  with x0=syscall_number, x1-x7=arguments
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

/* System Call Numbers (x0 register when calling svc #0) */
#define SYS_PRINT           1    /* Print string: args (char *str, u32 len) */
#define SYS_EXIT            2    /* Exit task: args (u32 status) */
#define SYS_GET_COUNTER     3    /* Get iteration counter: args (none) */
#define SYS_SLEEP           4    /* Sleep milliseconds: args (u32 ms) */
#define SYS_READ_CHAR       5    /* Read character from UART: args (none) */

/* Exception classes (from ESR_EL1[31:26]) */
#define EXC_UNKNOWN         0x00
#define EXC_SVC32           0x11  /* SVC in 32-bit mode */
#define EXC_SVC64           0x15  /* SVC in 64-bit mode (what we use!) */
#define EXC_DATA_ABORT      0x24  /* Data abort (memory fault) */

/* ARM64 Registers for syscall arguments */
typedef struct {
    u64 x0;  /* x0 = syscall number / return value */
    u64 x1;  /* arg0 / return value 2 */
    u64 x2;  /* arg1 */
    u64 x3;  /* arg2 */
    u64 x4;  /* arg3 */
    u64 x5;  /* arg4 */
    u64 x6;  /* arg5 */
    u64 x7;  /* arg6 */
    u64 x8;  /* temp */
} syscall_args_t;

/* Function declarations for syscall handlers (implemented in exceptions.c) */
void handle_svc_exception(void);
void handle_syscall(u64 syscall_number);

#endif /* SYSCALL_H */
