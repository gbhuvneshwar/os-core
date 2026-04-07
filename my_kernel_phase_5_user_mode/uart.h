#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char* s);
void uart_puthex(unsigned long n);
void uart_putdec(unsigned long n);
void uart_newline(void);
char uart_getc_nonblocking(void);  /* Non-blocking read */

#endif
