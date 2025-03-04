#ifndef _BIOS_H
#define _BIOS_H

#include "types.h"

extern void print_char(char c);
extern void print_string(char far *str, int add_newline);
extern char *my_ltoa(ulong num);
extern char *my_hex(ulong num, int len);

#endif /* _BIOS_H */
