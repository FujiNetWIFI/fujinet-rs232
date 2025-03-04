#include "bios.h"

/* ------ Utility functions --------------------------------------*/

/* screen output using simple BIOS TTY to minimize library code */
/* They are here to also allow you to insert display lines for */
/* debugging/investigation */

void print_char(char c)
{
  _asm {
    mov ah, 0x0e;
    mov al, byte ptr c;
    int 0x10;
  }
}

void print_string(char far *str, int add_newline)
{
  while (*str)
    print_char(*str++);
  if (add_newline) {
    print_char('\n');
    print_char('\r');
  }
}

char *my_ltoa(ulong num)
{
  static char buf[11] = "0000000000";
  int i;
  ulong tmp;

  buf[10] = 0;
  for (i = 0; i < 10; i++) {
    tmp = num / 10;
    buf[9 - i] = (char) ('0' + (num - (10 * tmp)));
    num = tmp;
    if (i && (!num))
      break;
  }
  return (char *) &buf[9 - i];
}

char *my_hex(ulong num, int len)
{
  static char buf[9];
  static char *hex = "0123456789ABCDEF";

  buf[len] = 0;
  while (len--) {
    buf[len] = hex[num & 0xf];
    num >>= 4;
  }
  return buf;
}

