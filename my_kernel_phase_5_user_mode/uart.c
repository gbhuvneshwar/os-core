/* PL011 UART registers for QEMU virt */
#define UART_BASE   0x09000000UL
#define UART_DR     (*(volatile unsigned int*)(UART_BASE + 0x00))
#define UART_FR     (*(volatile unsigned int*)(UART_BASE + 0x18))
#define UART_IBRD   (*(volatile unsigned int*)(UART_BASE + 0x24))
#define UART_LCRH   (*(volatile unsigned int*)(UART_BASE + 0x2C))
#define UART_CR     (*(volatile unsigned int*)(UART_BASE + 0x30))

/* flag register bits */
#define UART_FR_TXFF  (1 << 5)   /* transmit FIFO full */
#define UART_FR_RXFE  (1 << 4)   /* receive FIFO empty */

void uart_init(void) {
    /* disable UART */
    UART_CR   = 0;
    /* set baud rate 115200 */
    /* assuming 24MHz clock */
    UART_IBRD = 13;
    /* 8 bits, enable FIFO */
    UART_LCRH = (3 << 5) | (1 << 4);
    /* enable TX, RX, UART */
    UART_CR   = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c) {
    /* wait while TX FIFO full */
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)(unsigned char)c;
}

void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_puthex(unsigned long n) {
    const char* h = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 7; i >= 0; i--) {
        uart_putc(h[(n >> (i * 4)) & 0xF]);
    }
}

void uart_putdec(unsigned long n) {
    if (n == 0) {
        uart_putc('0');
        return;
    }
    char buf[21];
    int  i = 0;
    while (n > 0) {
        buf[i++] = '0' + (int)(n % 10);
        n = n / 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        uart_putc(buf[j]);
    }
}

void uart_newline(void) {
    uart_puts("\n");
}

char uart_getc_nonblocking(void) {
    /* Check if RX FIFO is empty */
    if (UART_FR & UART_FR_RXFE) {
        return 0;  /* Return 0 if no character available */
    }
    /* Read character from FIFO */
    return (char)(UART_DR & 0xFF);
}
