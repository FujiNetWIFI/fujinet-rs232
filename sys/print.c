#include "print.h"
#include <stdarg.h>
#include <ctype.h>

void printHex(uint16_t val, uint16_t width, char leading)
{
  uint16_t digits, tval;
  const char hex_digits[] = "0123456789ABCDEF";


  for (tval = val, digits = 0; tval; tval >>= 4, digits++)
    ;
  if (!digits)
    digits = 1;

  for (; digits < width; width--)
    printChar(leading);

  while (digits) {
    digits--;
    printChar(hex_digits[(val >> 4 * digits) & 0xf]);
  }

  return;
}

void printHex32(uint32_t val, uint16_t width, char leading)
{
  uint16_t digits;
  uint32_t tval;
  const char hex_digits[] = "0123456789ABCDEF";


  for (tval = val, digits = 0; tval; tval >>= 4, digits++)
    ;
  if (!digits)
    digits = 1;

  for (; digits < width; width--)
    printChar(leading);

  while (digits) {
    digits--;
    printChar(hex_digits[(val >> 4 * digits) & 0xf]);
  }

  return;
}

void printDec(uint16_t val, uint16_t width, char leading)
{
  uint16_t digits, tval, tens;


  for (tval = val, digits = 0; tval; tval /= 10, digits++)
    ;
  if (!digits)
    digits = 1;
  for (tval = digits - 1, tens = 1; tval; tval--, tens *= 10)
    ;

  for (; digits < width; width--)
    printChar(leading);

  while (digits) {
    digits--;
    printChar('0' + (val / tens) % 10);
    tens /= 10;
  }

  return;
}

void printDec32(uint32_t val, uint16_t width, char leading)
{
  uint32_t tval, tens;
  uint16_t digits;


  for (tval = val, digits = 0; tval; tval /= 10, digits++)
    ;
  if (!digits)
    digits = 1;
  for (tval = digits - 1, tens = 1; tval; tval--, tens *= 10)
    ;

  for (; digits < width; width--)
    printChar(leading);

  while (digits) {
    digits--;
    printChar('0' + (val / tens) % 10);
    tens /= 10;
  }

  return;
}

void dumpHex(void far *ptr, uint16_t count, uint16_t address)
{
  int outer, inner;
  uint8_t c, is_err;
  uint8_t far *buffer = (uint8_t far *) ptr;


  for (outer = 0; outer < count; outer += 16) {
    printHex(outer + address, 4, '0');
#ifdef DOS_SAFE
    printDTerm("  $");
#else
    printChar(' ');
    printChar(' ');
#endif /* DOS_SAFE */
    for (inner = 0; inner < 16; inner++) {
      if (inner + outer < count) {
        c = buffer[inner + outer];
	printHex(c, 2, '0');
	printChar(' ');
      }
      else {
#ifdef DOS_SAFE
	printDTerm("   $");
#else
	printChar(' ');
	printChar(' ');
	printChar(' ');
#endif /* DOS_SAFE */
      }
    }
#ifdef DOS_SAFE
    printDTerm(" |$");
#else
    printChar(' ');
    printChar('|');
#endif /* DOS_SAFE */
    for (inner = 0; inner < 16 && inner + outer < count; inner++) {
      c = buffer[inner + outer];
      if (c >= ' ' && c <= 0x7f)
	printChar(c);
      else
	printChar('.');
    }
#ifdef DOS_SAFE
    printDTerm("|\r\n$");
#else
    printChar('|');
    printChar('\r');
    printChar('\n');
#endif /* DOS_SAFE */
  }

  return;
}

void consolef(const char *format, ...)
{
  const char *pf;
  va_list args;
  char leader;
  uint8_t width;


  va_start(args, format);

  for (pf = format; pf && *pf; pf++) {
    switch (*pf) {
    case '\n':
#ifdef DOS_SAFE
      printDTerm("\r\n$");
#else
      printChar('\r');
      printChar('\n');
#endif /* DOS_SAFE */
      break;

    case '%':
      pf++;
      if (!*pf)
	break;

      if (*pf == 'c')
	printChar(va_arg(args, char));
      else {
	leader = ' ';
	width = 0;

	if (isdigit(*pf)) {
	  if (*pf == '0') {
	    leader = '0';
	    pf++;
	  }

	  for (width = 0; isdigit(*pf); pf++) {
	    width *= 10;
	    width += *pf - '0';
	  }
	}

	if (*pf == 'l') {
	  pf++;
	  if (*pf == 'x')
	    printHex32(va_arg(args, uint32_t), width, leader);
	  if (*pf == 'i' || *pf == 'd')
	    printDec32(va_arg(args, uint32_t), width, leader);
	}
	else {
	  if (*pf == 'x')
	    printHex(va_arg(args, uint16_t), width, leader);
	  if (*pf == 'i' || *pf == 'd')
	    printDec(va_arg(args, uint16_t), width, leader);
	}
      }
      break;

    default:
      printChar(*pf);
      break;
    }
  }

  va_end(args);

  return;
}
