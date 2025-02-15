#ifndef _PRINT_H
#define _PRINT_H

#include <stdint.h>

#define DOS_SAFE
#ifdef DOS_SAFE
/* Print a single character with DOS */
extern void printChar(char);
#pragma aux printChar =		\
  "mov ah, 0x2"			\
  "int 0x21"			\
  __parm [__dl]			\
  __modify [__ax __di __es];

/* Use INT 21h to print. Buffer MUST be terminated with '$' so BIOS
   knows when to stop */
extern void printDTerm(const char *);
#pragma aux printDTerm =      \
    "mov    ah, 0x9"          \
    "int    0x21"             \
    __parm [__dx]             \
    __modify [__ax __di __es];
#else /* !DOS_SAFE */
/* Print a single character with BIOS */
extern void printChar(char);
#pragma aux printChar =         \
  "mov ah, 0xE"                 \
  "int 0x10"                    \
  __parm [__al]                 \
  __modify [__ax __cx];
#endif /* DOS_SAFE */

extern void printHex(uint16_t val, uint16_t width, char leading);
extern void printHex32(uint32_t val, uint16_t width, char leading);
extern void printDec(uint16_t val, uint16_t width, char leading);
extern void dumpHex(void far *ptr, uint16_t count, uint16_t address);

extern void consolef(const char *format, ...);

#endif /* _PRINT_H */
