#ifndef _NETREDIR_H
#define _NETREDIR_H

#include "dosdata.h"
#include "fujifs.h"
#include <stdint.h>

#define DOS_INT_REDIR   0x2F
#define REDIRECTOR_FUNC 0x11

enum {
  SUBF_INQUIRY          = 0x00,
  SUBF_REMOVEDIR        = 0x01,
  SUBF_MAKEDIR          = 0x03,
  SUBF_CHDIR            = 0x05,
  SUBF_CLOSE            = 0x06,
  SUBF_COMMIT           = 0x07,
  SUBF_READ             = 0x08,
  SUBF_WRITE            = 0x09,
  SUBF_LOCK             = 0x0A,
  SUBF_UNLOCK           = 0x0B,
  SUBF_GETDISKSPACE     = 0x0C,
  SUBF_SETATTR          = 0x0E,
  SUBF_GETATTR          = 0x0F,
  SUBF_RENAME           = 0x11,
  SUBF_DELETE           = 0x13,
  SUBF_OPENEXIST        = 0x16,
  SUBF_OPENCREATE       = 0x17,
  SUBF_FINDFIRST        = 0x1B,
  SUBF_FINDNEXT         = 0x1C,
  SUBF_CLOSEALL         = 0x1D,
  SUBF_DOREDIR          = 0x1E,
  SUBF_PRINTERSETUP     = 0x1F,
  SUBF_FLUSHBUFFERS     = 0x20,
  SUBF_SEEK             = 0x21,
  SUBF_PROCTERM         = 0x22,
  SUBF_QUALIFYPATH      = 0x23,
  SUBF_REDIRPRINTER     = 0x25,
  SUBF_OPENEXTENDED     = 0x2E,
};

extern void interrupt far (*old_int2f)();
extern void far *sda_ptr;
extern uint8_t fn_drive_num;
extern fujifs_handle fn_host;
extern char *fn_volume;
extern char fn_cwd[];

#if 0
extern char fuji_cwd[];
#endif

extern void interrupt far redirector(union INTPACK regs);

// FIXME - these don't belong here
extern __segment getCS(void);
#pragma aux getCS = \
    "mov ax, cs";

extern __segment getDS(void);
#pragma aux getDS = \
    "mov ax, ds";

extern __segment getSS(void);
#pragma aux getSS = \
    "mov ax, ss";

extern __segment getBP(void);
#pragma aux getBP = \
    "mov ax, bp";

#endif /* _NETREDIR_H */
