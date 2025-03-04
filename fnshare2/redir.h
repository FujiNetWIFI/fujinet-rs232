#ifndef _REDIR_H
#define _REDIR_H

#include "dosdata.h"
#include "fujifs.h"

typedef void (interrupt far *INTVECT)();

extern INTVECT prev_int2f_vector;

extern uint8_t fn_drive_num;
extern fujifs_handle fn_host;
extern char *fn_volume;
extern char fn_cwd[];

// FIXME - evil globals
extern DIRREC_PTR dirrec_ptr1;
extern DIRREC_PTR dirrec_ptr2;
extern SRCHREC_PTR srchrec_ptr1;
extern SRCHREC_PTR srchrec_ptr2;
extern char far *cds_path_root;
extern char far *current_path;
extern char far *fcbname_ptr1;
extern char far *fcbname_ptr2;
extern char far *filename_ptr1;
extern char far *filename_ptr2;
//extern char our_drive_str[];
extern uint16_t cds_root_size;
extern uint8_t far *sda_ptr;
extern uint8_t far *srch_attr_ptr;

/* The following structure is compiler specific, and maps
        onto the registers pushed onto the stack for an interrupt
        function. */
typedef struct {
#ifdef __BORLANDC__
  uint16_t bp, di, si, ds, es, dx, cx, bx, ax;
#else
#ifdef __WATCOMC__
  uint16_t gs, fs;
#endif /* __WATCOMC__ */
  uint16_t es, ds, di, si, bp, sp, bx, dx, cx, ax;
#endif
  uint16_t ip, cs, flags;
} ALL_REGS;

extern void interrupt far redirector(ALL_REGS entry_regs);

#endif /* _REDIR_H */
