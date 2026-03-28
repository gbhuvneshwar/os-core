#define UART0_BASE   0x09000000
#define SYSCON_BASE  0x09080000    /* power control */
#define SYSCON_POWEROFF 0x5555     /* magic value to poweroff */

void uart_putc(char c) {
    volatile char* uart = (volatile char*)UART0_BASE;
    *uart = c;
}

void uart_puts(const char* str) {
    while (*str) uart_putc(*str++);
}

void qemu_poweroff(void) {
    /* write magic value to SYSCON */
    /* tells QEMU to power off! */
    volatile int* syscon = (volatile int*)SYSCON_BASE;
    *syscon = SYSCON_POWEROFF;
}

void kernel_main(void) {
    uart_puts("===========================\r\n");
    uart_puts("  MY KERNEL IS RUNNING!   \r\n");
    uart_puts("===========================\r\n");
    uart_puts("Hello from bare metal!\r\n");
    uart_puts("No Linux! No macOS!\r\n");
    uart_puts("Just MY code on ARM64!\r\n");
    uart_puts("===========================\r\n");
    uart_puts("Shutting down QEMU...\r\n");

    /* power off QEMU automatically! */
    /* no need to press anything! */
    qemu_poweroff();

    /* should not reach here */
    while (1) {}
}