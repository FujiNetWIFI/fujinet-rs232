#ifndef _REDIR_H
#define _REDIR_H

#include "types.h"

typedef void (interrupt far *INTVECT)();

extern INTVECT prev_int2f_vector;

extern uint cds_root_size;
extern uchar our_drive_no;
extern char far *cds_path_root;
extern char our_drive_str[];
extern char far *fcbname_ptr;
extern uchar far *sda_ptr;
extern char far *current_path;
extern char far *filename_ptr;
extern char far *filename_ptr_2;
extern char far *fcbname_ptr_2;
extern uchar far *srch_attr_ptr;
extern SRCHREC_PTR srchrec_ptr;
extern SRCHREC_PTR srchrec_ptr_2;
extern DIRREC_PTR dirrec_ptr;
extern DIRREC_PTR dirrec_ptr_2;

extern void interrupt far redirector(ALL_REGS entry_regs);

#endif /* _REDIR_H */
