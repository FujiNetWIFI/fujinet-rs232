#include "netredir.h"
#include "fujicom.h"
#include <stdint.h>
#include <stdlib.h>
#include <dos.h>

void interrupt far (*old_int2f)();

typedef uint16_t(*redirectFunction_t)(void far *parms);

static redirectFunction_t dispatchTable[] = {
  NULL,		// 0x00 inquiry
  NULL,		// 0x01 remove directory
  NULL,		// 0x02 ?
  NULL,		// 0x03 make directory
  NULL,		// 0x04 ?
  NULL,		// 0x05 current directory
  NULL,		// 0x06 close file
  NULL,		// 0x07 commit file (flush buffer?)
  NULL,		// 0x08 read file
  NULL,		// 0x09 write file
  NULL,		// 0x0A lock region of file
  NULL,		// 0x0B unlock region of file
  NULL,		// 0x0C get disk space
  NULL,		// 0x0D ?
  NULL,		// 0x0E set file attributes
  NULL,		// 0x0F get file attributes
  NULL,		// 0x10 ?
  NULL,		// 0x11 rename file
  NULL,		// 0x12 ?
  NULL,		// 0x13 delete file
  NULL,		// 0x14 ?
  NULL,		// 0x15 ?
  NULL,		// 0x16 open existing file
  NULL,		// 0x17 create/truncate file
  NULL,		// 0x18 ?
  NULL,		// 0x19 ?
  NULL,		// 0x1A ?
  NULL,		// 0x1B find first matching file
  NULL,		// 0x1C find next matching file
  NULL,		// 0x1D close all files for process
  NULL,		// 0x1E do redirection
  NULL,		// 0x1F printer setup
  NULL,		// 0x20 flush all buffers
  NULL,		// 0x21 seek from end of file
  NULL,		// 0x22 process termination hook
  NULL,		// 0x23 qualify path and filename
  NULL,		// 0x24 ?
  NULL,		// 0x25 redirected printer mode
  NULL,		// 0x26 ?
  NULL,		// 0x27 ?
  NULL,		// 0x28 ?
  NULL,		// 0x29 ?
  NULL,		// 0x2A ?
  NULL,		// 0x2B ?
  NULL,		// 0x2C ?
  NULL,		// 0x2D ?
  NULL,		// 0x2E extended open file
};

/* Print a single character with BIOS */
extern void printChar(char);
#pragma aux printChar =         \
  "mov ah, 0xE"                 \
  "int 0x10"                    \
  __parm [__al]                 \
  __modify [__ax __cx];

static inline void printHex32(uint32_t val, uint16_t width, char leading)
{
  uint16_t digits;
  uint32_t tval;
  char c;


  for (tval = val, digits = 0; tval; tval >>= 4, digits++)
    ;
  if (!digits)
    digits = 1;

  for (; digits < width; width--)
    printChar(leading);

  while (digits) {
    digits--;
    c = (val >> 4 * digits) & 0xf;
    printChar('0' + c + (c > 9 ? 7 : 0));
  }

  return;
}

void interrupt far redirector(union INTPACK regs)
{
  uint8_t func, subfunc;


  func = regs.h.ah;
  subfunc = regs.h.al;

  printChar('F');
  printHex32(getCS(), 4, '0');
  printChar('-');
  printHex32(getDS(), 4, '0');
  printChar(':');
  printHex32(regs.x.ax, 4, '0');
  printChar('.');
  //consolef("AX: %04x\n", regs.x.ax);
  
  if (func != REDIRECTOR_FUNC) { // Not our redirector call, pass it down the chain
    _chain_intr(old_int2f);
    return; // Should never reach here
  }
  printChar('N');

  // FIXME - check if drive is us
  //consolef("FN REDIRECT SUB 0x%02x\n", subfunc);
  _chain_intr(old_int2f);

  return;
}
