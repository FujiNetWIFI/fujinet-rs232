#ifndef _REDIR_H
#define _REDIR_H

#include "dosdata.h"

typedef void (interrupt far *INTVECT)();

extern INTVECT prev_int2f_vector;

extern uint16_t cds_root_size;
extern uint8_t our_drive_no;
extern char far *cds_path_root;
extern char our_drive_str[];
extern char far *fcbname_ptr;
extern uint8_t far *sda_ptr;
extern char far *current_path;
extern char far *filename_ptr;
extern char far *filename_ptr_2;
extern char far *fcbname_ptr_2;
extern uint8_t far *srch_attr_ptr;
extern SRCHREC_PTR srchrec_ptr;
extern SRCHREC_PTR srchrec_ptr_2;
extern DIRREC_PTR dirrec_ptr;
extern DIRREC_PTR dirrec_ptr_2;

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
