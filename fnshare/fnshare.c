#include "netredir.h"
#include "psp.h"
#include <dos.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

extern void dumpHex(void far *ptr, uint16_t count);

//#pragma data_seg("BEGTEXT", "CODE");
//#pragma code_seg("BEGTEXT", "CODE");

#define DRIVE_LETTER 'Z'

//#pragma code_seg("_INIT", "INIT")

int main()
{
  uint16_t end;
  extern void redirector_vect();
  psp_t *psp;


  // FIXME - parse arguments

  printf("Installing FujiNet redirector on drive %c:\n", DRIVE_LETTER);
  old_int2f = _dos_getvect(DOS_INT_REDIR);
  printf("Old vector: 0x%08lx\n", (uint32_t) old_int2f);
  //_dos_setvect(DOS_INT_REDIR, MK_FP(getCS(), redirector_vect));
  _dos_setvect(DOS_INT_REDIR, redirector);
  //_dos_setvect(0x60, redirector);

  // FIXME - allocate drive letter

  psp = (psp_t *) _psp;
  dumpHex(psp, sizeof(*psp));

  // Become a TSR
  printf("Main: 0x%08lx  Redir: 0x%08lx\n", (uint32_t) FP_SEG(main),
	 (uint32_t) FP_SEG(redirector));
  end = (FP_SEG(main) - getCS()) << 4;
  end += 256; // size of PSP
  end += 15;
  printf("Keep size: 0x%04X-0x%04x 0x%04x %i %i\n", getCS(), psp->top_segment, _psp,
	 sizeof(*psp), end);
  _dos_keep(0, end >> 4);
  
  return 0;
}
