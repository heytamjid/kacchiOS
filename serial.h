/* serial.h - Serial port driver interface */
#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char* str);
char serial_getc(void);
void serial_put_hex(uint32_t value);
void serial_put_dec(uint32_t value);

#endif