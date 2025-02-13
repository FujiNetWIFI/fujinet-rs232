#include "netredir.h"
#include "dosdata.h"
#include <dos.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void dumpHex(void far *ptr, uint16_t count);

//#pragma data_seg("BEGTEXT", "CODE");
//#pragma code_seg("BEGTEXT", "CODE");

#define DRIVE_LETTER 'L'

//#pragma code_seg("_INIT", "INIT")
static union REGS regs;
struct SREGS segr;

int main()
{
  uint16_t end;
  LOLREC_PTR lolptr;              /* pointer to List Of Lists */


  // FIXME - parse arguments

  // FIXME - allocate drive letter
  regs.x.ax = 0x5D06;
  intdosx(&regs, &regs, &segr);
  sda_ptr = MK_FP(segr.ds, regs.x.si);
  printf("SDA: 0x%08lx\n", (uint32_t) sda_ptr);

  regs.x.ax = 0x5200;
  intdosx(&regs, &regs, &segr);
  lolptr = (LOLREC_PTR) MK_FP(segr.es, regs.x.bx);
  printf("LOL: 0x%08lx\n", (uint32_t) lolptr);
  dumpHex(lolptr, sizeof(*lolptr));

  {
    V3_CDS_PTR our_cds_ptr;
    int our_drive_no = DRIVE_LETTER - 'A';
    uint16_t cds_root_size;             /* Size of our CDS root string */
    char far *current_path;         /* ptr to current path in CDS */
    char far *cds_path_root = "FujiNet  :\\";       /* Root string for CDS */


    our_cds_ptr = lolptr->cds_ptr;
    if (_osmajor == 3)
      our_cds_ptr = our_cds_ptr + our_drive_no;
    else {
      V4_CDS_PTR t = (V4_CDS_PTR) our_cds_ptr;


      t = t + our_drive_no;
      our_cds_ptr = (V3_CDS_PTR) t;
    }

    if (our_drive_no >= lolptr->last_drive) {
      printf("Drive letter %c higher than last drive %c",
             our_drive_no + 'A', lolptr->last_drive + 'A');
      exit(1);
    }

    // Check that this drive letter is currently invalid (not in use already)
    // 0xc000 tests both physical and network bits at same time
    if ((our_cds_ptr->flags & 0xc000) != 0) {
      printf("Drive already assigned...");
      exit(1);
    }

    // Set Network+Physical+NotRealNetworkDrive bits on, and
    // establish our 'root'
    our_cds_ptr->flags |= 0xc000;
    cds_root_size = _fstrlen(cds_path_root);
    _fstrcpy(our_cds_ptr->current_path, cds_path_root);
    our_cds_ptr->current_path[_fstrlen(our_cds_ptr->current_path) - 3] =
      (char) ('A' + our_drive_no);
    _fstrcpy(cds_path_root, our_cds_ptr->current_path);
    current_path = our_cds_ptr->current_path;
    our_cds_ptr->root_ofs = _fstrlen(our_cds_ptr->current_path) - 1;
    current_path += our_cds_ptr->root_ofs;
  }

  printf("Installing FujiNet redirector on drive %c:\n", DRIVE_LETTER);
  old_int2f = _dos_getvect(DOS_INT_REDIR);
  printf("Old vector: 0x%08lx\n", (uint32_t) old_int2f);
  _dos_setvect(DOS_INT_REDIR, redirector);

  // Become a TSR
  end = (FP_SEG(main) - getCS()) << 4;
  end += 256; // size of PSP
  end += 15;
  _dos_keep(0, end >> 4);

  return 0;
}
