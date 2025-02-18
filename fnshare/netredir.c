#include "netredir.h"
#include "dosdata.h"
#include "fujicom.h"
#include "../ncopy/fujifs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "../sys/print.h"

/* Get value of field from [S]wappable [D]OS [A]rea */
#define DOS_SDA_VALUE(x) DOS_STRUCT_VALUE(SDA_PTR, sda_ptr, x)
/* Get pointer to field */
#define DOS_SDA_POINTER(x) DOS_STRUCT_POINTER(SDA_PTR, sda_ptr, x)

void interrupt far (*old_int2f)();
void far *sda_ptr;
uint8_t drive_num;
static uint16_t dir_counter;

typedef uint16_t(*redirectFunction_t)(void far *parms);

#if 0
static redirectFunction_t dispatchTable[] = {
  NULL,         // 0x00 inquiry
  NULL,         // 0x01 remove directory
  NULL,         // 0x02 ?
  NULL,         // 0x03 make directory
  NULL,         // 0x04 ?
  NULL,         // 0x05 current directory
  NULL,         // 0x06 close file
  NULL,         // 0x07 commit file (flush buffer?)
  NULL,         // 0x08 read file
  NULL,         // 0x09 write file
  NULL,         // 0x0A lock region of file
  NULL,         // 0x0B unlock region of file
  NULL,         // 0x0C get disk space
  NULL,         // 0x0D ?
  NULL,         // 0x0E set file attributes
  NULL,         // 0x0F get file attributes
  NULL,         // 0x10 ?
  NULL,         // 0x11 rename file
  NULL,         // 0x12 ?
  NULL,         // 0x13 delete file
  NULL,         // 0x14 ?
  NULL,         // 0x15 ?
  NULL,         // 0x16 open existing file
  NULL,         // 0x17 create/truncate file
  NULL,         // 0x18 ?
  NULL,         // 0x19 ?
  NULL,         // 0x1A ?
  NULL,         // 0x1B find first matching file
  NULL,         // 0x1C find next matching file
  NULL,         // 0x1D close all files for process
  NULL,         // 0x1E do redirection
  NULL,         // 0x1F printer setup
  NULL,         // 0x20 flush all buffers
  NULL,         // 0x21 seek from end of file
  NULL,         // 0x22 process termination hook
  NULL,         // 0x23 qualify path and filename
  NULL,         // 0x24 ?
  NULL,         // 0x25 redirected printer mode
  NULL,         // 0x26 ?
  NULL,         // 0x27 ?
  NULL,         // 0x28 ?
  NULL,         // 0x29 ?
  NULL,         // 0x2A ?
  NULL,         // 0x2B ?
  NULL,         // 0x2C ?
  NULL,         // 0x2D ?
  NULL,         // 0x2E extended open file
};
#endif

void set_intr_retval(uint16_t);
#pragma aux set_intr_retval = \
  "mov ss:[bp+22],ax" \
  "test ax,ax"	      \
  "jnz set_carry"     \
  "clc"		      \
  "jmp done"	      \
  "set_carry: stc"    \
  "done:"	      \
  parm [ax]

void interrupt far redirector(union INTPACK regs)
{
  uint8_t func, subfunc;
  char far *path;
  uint16_t result;


  func = regs.h.ah;
  subfunc = regs.h.al;
  //  consolef("FN REDIR 0x%02x/%02x\n", func, subfunc);
  //consolef("");

  if (func != REDIRECTOR_FUNC) { // Not our redirector call, pass it down the chain
    _chain_intr(old_int2f);
    return; // Should never reach here
  }

  if (subfunc == SUBF_INQUIRY) {
    set_intr_retval(0xff);
    return;
  }

  // FIXME - check if drive is us

  // Check if path is us
  path = DOS_SDA_VALUE(cdsptr);
  if (_fstrncmp(path, "FujiNet ", 8) != 0)
    goto not_us;

  switch (subfunc) {
  case SUBF_FINDFIRST:
    result = findfirst(path, DOS_SDA_POINTER(srchrec));
    break;

  case SUBF_FINDNEXT:
    result = findnext(DOS_SDA_POINTER(srchrec));
    break;

  default:
    consolef("FN REDIRECT SUB 0x%02x ES: 0x%04x DI: 0x%04x\n", subfunc, regs.x.es, regs.x.di);
    result = 0x16;
    break;
  }

  consolef("RESULT: 0x%04x\n", result);
  set_intr_retval(result);
  return;

 not_us:
  _chain_intr(old_int2f);
}

int findfirst(const char far *path, SRCHREC_PTR search)
{
  errcode err;
  DIRREC_PTR dos_entry;
  char far *pattern;
  uint8_t search_attr;


  consolef("PATH: %ls\n", DOS_SDA_VALUE(file_name));
  // FIXME - make these arguments instead of accessing globals
  dos_entry = DOS_SDA_POINTER(dirrec);
  pattern = DOS_SDA_VALUE(fcb_name);
  search_attr = DOS_SDA_VALUE(srch_attr);
  
  consolef("FF PATTERN: 0x%02x %ls\n", search_attr, pattern);

  if (search_attr & ATTR_VOLUME_LABEL) {
    consolef("VOLUME\n");
    search->drive_num = drive_num | 0x80;
    search->sequence = 0;
    search->sector = 0;
    _fstrcpy(dos_entry->name, "FUJINET1234");
    dos_entry->attr = 0x08;
    dos_entry->size = 0;
    return 0; // FIXME - use constant
  }

  // FIXME - was directory already open?
  //fujifs_closedir();

  err = fujifs_opendir();
  if (err)
    return 0x38; // Unexpected network error - FIXME - use constant

  consolef("FORWARDING\n");
  return 0;
  dir_counter = 0;
  return findnext(search);
}

int findnext(SRCHREC_PTR search)
{
  FN_DIRENT *ent;
  int len;
  char far *pattern;
  uint8_t search_attr;
  DIRREC_PTR dos_entry;


  // FIXME - make these arguments instead of accessing globals
  dos_entry = DOS_SDA_POINTER(dirrec);
  pattern = DOS_SDA_VALUE(fcb_name);
  search_attr = search->attr_mask;

  consolef("NX PATTERN: %ls\n", pattern);
  ent = fujifs_readdir();
  search->drive_num = drive_num;
  _fmemset(search->srch_mask, ' ', sizeof(search->srch_mask));
  len = strlen(ent->name);
  _fmemcpy(search->srch_mask, ent->name, len <= 11 ? len : 11);
  search->attr_mask = 0x3f;
  search->sequence = dir_counter++;
  search->sector = 0;

  return 0;
}
