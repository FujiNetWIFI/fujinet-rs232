#ifndef _BIOS_H
#define _BIOS_H

#include "dosdata.h"

extern void print_char(char c);
extern void print_string(char far *str, int add_newline);
extern char *my_ltoa(uint32_t num);
extern char *my_hex(uint32_t num, int len);

#endif /* _BIOS_H */
