/*
 * user_task.c - User Mode Task Implementations
 *
 * Contains code that runs in USER MODE (EL0)
 * These tasks make syscalls to access kernel services
 */

#include "usermode.h"

/* Helper to print a string via syscall */
static void user_print(const char *msg, int len) {
    register u64 x0 asm("x0") = 1;      /* SYS_PRINT */
    register u64 x1 asm("x1") = (u64)msg;
    register u64 x2 asm("x2") = len;
    
    asm volatile(
        "svc #0"
        : "+r"(x0), "+r"(x1), "+r"(x2)
        :
        :
    );
}

/* Helper to exit */
static void user_exit(int code) {
    register u64 x0 asm("x0") = 2;      /* SYS_EXIT */
    register u64 x1 asm("x1") = code;
    
    asm volatile(
        "svc #0"
        : "+r"(x0), "+r"(x1)
        :
        :
    );
}

/*
 * User Task A - Runs in EL0
 */
void user_task_a(void) {
    const char msg1[] = "USER TASK A - Running in EL0\n";
    user_print(msg1, 29);
    
    const char msg2[] = "Task A: Syscall in action!\n";
    user_print(msg2, 27);
    
    for (volatile int i = 0; i < 2; i++) {
        const char work[] = "A: Work iteration...\n";
        user_print(work, 21);
    }
    
    const char exit_msg[] = "Task A: Exiting\n";
    user_print(exit_msg, 15);
    
    user_exit(0);
    while (1) {}
}

/*
 * User Task B - Also runs in EL0
 */
void user_task_b(void) {
    const char msg[] = "USER TASK B - Alternative task\n";
    user_print(msg, 31);
    
    const char work[] = "B: Processing work...\n";
    user_print(work, 23);
    
    const char done[] = "B: Exiting now\n";
    user_print(done, 15);
    
    user_exit(0);
    while (1) {}
}

/*
 * User Task C - Demonstrates syscall usage
 */
void user_task_c(void) {
    const char msg[] = "TASK C: Syscall demonstration\n";
    user_print(msg, 30);
    
    const char msg2[] = "EL0 to EL1 transition via SVC\n";
    user_print(msg2, 31);
    
    const char msg3[] = "C: Finished, exiting\n";
    user_print(msg3, 21);
    
    user_exit(42);
    while (1) {}
}

/*
 * Simple user task
 */
void simple_user_task(void) {
    const char msg[] = "SIMPLE USER TASK IN EL0!\n";
    user_print(msg, 26);
    user_exit(0);
    while (1) {}
}
