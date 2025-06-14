#include "dosfunc.h"
#include <dos.h>

/* ------------------- Internal DOS calls ------------ */

#if NEED_DOS_CALLS
#if 0
uint16_t r_es, r_di, r_ds;
#endif
uint16_t result_lo, result_hi;

uint32_t dos_ftime(void)
{
#if 0
  r_es = r.es, r_di = r.di, r_ds = r.ds;

  _asm {
    mov save_sp, sp;    /* Save current stack pointer. */
    cli;
    mov ss, dos_ss;     /* Establish DOS's stack, current */
    mov sp, dos_sp;     /* when we got called. */
    sti;
    mov ax, 0x120d;     /* Get time/date. */
    push di;            /* Subfunction 120C destroys di */
    push ds;            /* It needs DS to be DOS's DS, for DOS 5.0 */
    push es;
    mov es, r_es;
    mov di, r_di;
    mov ds, r_ds;
    int 0x2F;
    xchg ax, dx;
    pop es;
    pop ds;             /* Restore DS */
    pop di;
    mov bx, ds;         /* Restore SS (same as DS) */
    cli;
    mov ss, bx;
    mov sp, save_sp;    /* and stack pointer (which we saved). */
    sti;
    mov result_lo, ax;
    mov result_hi, dx;
  }
#else
#warning getting the time does not work
#endif

  return (uint32_t) MK_FP(result_hi, result_lo);
}

void set_sft_owner(SFTREC_PTR sft)
{
#if 0
  r_ds = r.ds;

  _asm {
    push es;
    push di;
    les di, sft;
    mov save_sp, sp;    /* Save current stack pointer. */
    cli;
    mov ss, dos_ss;     /* Establish DOS's stack, current */
    mov sp, dos_sp;     /* when we got called. */
    sti;
    mov ax, 0x120c;     /* Claim file as ours. */
    push ds;            /* It needs DS to be DOS's DS, for DOS 5.0 */
    mov ds, r_ds;
    int 0x2F;
    pop bx;             /* Restore DS */
    mov ds, bx;         /* Restore SS (same as DS) */
    cli;
    mov ss, bx;
    mov sp, save_sp;    /* and stack pointer (which we saved). */
    sti;
    pop di;
    pop es;
  }
#else
#warning seting owner does not work
#endif
}
#endif /* NEED_DOS_CALLS */

// Does fcbname_ptr point to a device name?
int is_a_character_device(uint16_t dos_ds)
{
  int result;

  _asm {
    mov ax, 0x1223;     /* Search for device name. */
    push ds;            /* It needs DS to be DOS's DS, for DOS 5.0 */
    mov ds, dos_ds;
    int 0x2F;
    pop ds;             /* Restore DS */
    jnc is_indeed;
    mov result, FALSE;
    jmp done;
 is_indeed:
    mov result, TRUE;
  done:
  }

  return result;
}

/* See whether the filename matches the mask, one character
   position at a time. A wildcard ? in tha mask matches any
   character in the filename, any other character in the mask,
   including spaces, must match exactly */

int match_to_mask(char far *mask, char far *filename)
{
  int i;

  for (i = 0; i < 11; i++)
    if ((mask[i] != filename[i]) && (mask[i] != '?'))
      return FALSE;

  return TRUE;
}
