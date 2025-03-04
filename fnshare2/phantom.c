/*******************************************************

 PHANTOM.C - A network-redirector based XMS Ram Disk
 Copyright (c) David Maxey 1993.  All rights reserved.
 From "Undocumented DOS", 2nd edition (Addison-Wesley, 1993)

 Much of this code is explained in depth in Undocumented
 DOS, 2nd edition, chapter 8, which has a 60-page description
 of the network redirector, and an in-depth examination of
 how Phantom handles file read, open, ffirst, cd, and md.
 UndocDOS also contains a full specification for the redirector
 interface.

 1993-Jul-25 - Drive number should be one less. AES found using
               FILES/USEFCB. Porbably same as Markun/LanMan
               problem. Search for ref: DR_TOO_HIGH

 2025-Feb-26 - Converted to compile with Open Watcom - FozzTexx

*******************************************************/

#include "bios.h"
#include "dosfunc.h"
#include "xms.h"
#include "ramdrive.h"
#include "redir.h"
#include <stdlib.h>
#include <dos.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <bios.h>
#include <time.h>

#define DEBUG
#ifdef DEBUG
#include "../../fujinet-rs232/sys/print.h"
#endif

/* ****************************************************
   Constants and Macros
   **************************************************** */

#define         ROOTDIR_ENTRIES         128

#ifndef MK_FP
#define MK_FP(a,b)  ((void far *)(((ulong)(a) << 16) | (b)))
#endif

char *signon_string =
  "\r\n"
  "PHANTOM: A Network-Redirector Based XMS Ram Disk\r\n"
  "Copyright (c) David Maxey 1993.  All rights reserved.\r\n"
  "From \"Undocumented DOS\", 2nd edition (Addison-Wesley, 1993)\r\n";

char *usage_string =
  "Usage:\r\n"
  "    PHANTOM [-Snnnn] d:\r\n"
  " Or\r\n"
  "    PHANTOM -U\r\n"
  "\r\n"
  "where:\r\n"
  "    -Snnnn  specifies size of Ram Disk in kb of XMS\r\n"
  "    d:      specifies drive letter to use\r\n"
  "    -U      unloads the latest copy of Phantom loaded\r\n";

/* ************************************************
   Global data declarations
   ************************************************ */

/* This is declared in the compiler startup code to mark the
        end of the data segment. */
extern uint end;

/* Other global data items */
SIGREC sigrec = { 8, "PHANTOM ", 0, 0, 0 };     /* Signature record */
LOLREC_PTR lolptr;              /* pointer to List Of Lists */

/* ------ File system functions ------------------------ */

/* Fail Phantom, print message, exit to DOS */
void failprog(char *msg)
{
  if (xms_handle)
    xms_free_block(xms_handle);
  print_string((uchar far *) msg, TRUE);
  exit(1);
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

/* ---- Utility and startup functions --------------*/

/* Deal with differences in DOS version once, and set up a set
        of absolute pointers */

void set_up_pointers(void)
{
  if (_osmajor == 3) {
    fcbname_ptr = ((V3_SDA_PTR) sda_ptr)->fcb_name;
    filename_ptr = ((V3_SDA_PTR) sda_ptr)->file_name + cds_root_size - 1;
    fcbname_ptr_2 = ((V3_SDA_PTR) sda_ptr)->fcb_name_2;
    filename_ptr_2 = ((V3_SDA_PTR) sda_ptr)->file_name_2 + cds_root_size - 1;
    srchrec_ptr = &((V3_SDA_PTR) sda_ptr)->srchrec;
    dirrec_ptr = &((V3_SDA_PTR) sda_ptr)->dirrec;
    srchrec_ptr_2 = &((V3_SDA_PTR) sda_ptr)->rename_srchrec;
    dirrec_ptr_2 = &((V3_SDA_PTR) sda_ptr)->rename_dirrec;
    srch_attr_ptr = &((V3_SDA_PTR) sda_ptr)->srch_attr;
  }
  else {
    fcbname_ptr = ((V4_SDA_PTR) sda_ptr)->fcb_name;
    filename_ptr = ((V4_SDA_PTR) sda_ptr)->file_name + cds_root_size - 1;
    fcbname_ptr_2 = ((V4_SDA_PTR) sda_ptr)->fcb_name_2;
    filename_ptr_2 = ((V4_SDA_PTR) sda_ptr)->file_name_2 + cds_root_size - 1;
    srchrec_ptr = &((V4_SDA_PTR) sda_ptr)->srchrec;
    dirrec_ptr = &((V4_SDA_PTR) sda_ptr)->dirrec;
    srchrec_ptr_2 = &((V4_SDA_PTR) sda_ptr)->rename_srchrec;
    dirrec_ptr_2 = &((V4_SDA_PTR) sda_ptr)->rename_dirrec;
    srch_attr_ptr = &((V4_SDA_PTR) sda_ptr)->srch_attr;
  }
}

/* Get DOS version, address of Swappable DOS Area, and address of
        DOS List of lists. We only run on versions of DOS >= 3.10, so
        fail otherwise */

void get_dos_vars(void)
{
  uint segmnt;
  uint ofset;

  if ((_osmajor < 3) || ((_osmajor == 3) && (_osminor < 10)))
    failprog("Unsupported DOS Version");

  _asm {
    push ds;
    push es;
    mov ax, 0x5d06;     /* Get SDA pointer */
    int 0x21;
    mov segmnt, ds;
    mov ofset, si;
    pop es;
    pop ds;
  }

  sda_ptr = MK_FP(segmnt, ofset);

  _asm {
    push ds;
    push es;
    mov ax, 0x5200;     /* Get Lol pointer */
    int 0x21;
    mov segmnt, es;
    mov ofset, bx;
    pop es;
    pop ds;
  }

  lolptr = (LOLREC_PTR) MK_FP(segmnt, ofset);
}

/* Check to see that we are allowed to install */
void is_ok_to_load(void)
{
  int result;

  _asm {
    mov ax, 0x1100;
    int 0x2f;
    mov result, ax;
  }

  if (result == 1)
    failprog("Not OK to install a redirector...");
  return;
}

/* This is where we do the initializations of the DOS structures
        that we need in order to fit the mould */

void set_up_cds(void)
{
  V3_CDS_PTR our_cds_ptr;

  our_cds_ptr = lolptr->cds_ptr;
  if (_osmajor == 3)
//              our_cds_ptr = our_cds_ptr + (our_drive_no - 1);  // ref: DR_TOO_HIGH
    our_cds_ptr = our_cds_ptr + our_drive_no;
  else {
    V4_CDS_PTR t = (V4_CDS_PTR) our_cds_ptr;

//              t = t + (our_drive_no - 1);  // ref: DR_TOO_HIGH
    t = t + our_drive_no;
    our_cds_ptr = (V3_CDS_PTR) t;
  }

//      if (our_drive_no > lolptr->last_drive)  // ref: DR_TOO_HIGH
  if (our_drive_no >= lolptr->last_drive)
    failprog("Drive letter higher than last drive.");

  // Check that this drive letter is currently invalid (not in use already)
  // 0xc000 tests both physical and network bits at same time
  if ((our_cds_ptr->flags & 0xc000) != 0)
    failprog("Drive already assigned...");

  // Set Network+Physical+NotRealNetworkDrive bits on, and
  // establish our 'root'
  our_cds_ptr->flags |= 0xc080;
  cds_root_size = _fstrlen(cds_path_root);
  _fstrcpy(our_cds_ptr->current_path, cds_path_root);
  our_cds_ptr->current_path[_fstrlen(our_cds_ptr->current_path) - 3] =
//              (char) ('@'+ our_drive_no);  // ref: DR_TOO_HIGH
    (char) ('A' + our_drive_no);
  _fstrcpy(cds_path_root, our_cds_ptr->current_path);
  current_path = our_cds_ptr->current_path;
  our_cds_ptr->root_ofs = _fstrlen(our_cds_ptr->current_path) - 1;
  current_path += our_cds_ptr->root_ofs;
}

/* ---- Unload functionality --------------*/

/* Find the latest Phantom installed, unplug it from the Int 2F
        chain if possible, make the CDS reflect an invalid drive, and
        free its real and XMS memory. */

static uint ul_save_ss, ul_save_sp;
static int ul_i;

void exit_ret()
{
  _asm {
    // We should arrive back here - restore SS:SP
    mov ax, seg ul_save_ss;
    mov ds, ax;
    mov ss, ul_save_ss;
    mov sp, ul_save_sp;

    // restore the registers
    pop bp;
    pop di;
    pop si;
    pop ds;
    pop es;

    // Set current PSP back to us.
    mov bx, _psp;
    mov ah, 0x50;
    int 0x21;
  }

  _dos_setvect(ul_i, NULL);
//      our_drive_str[0] = (char) (our_drive_no + '@');  // ref: DR_TOO_HIGH
  our_drive_str[0] = (char) (our_drive_no + 'A');
  print_string(our_drive_str, FALSE);
  print_string(" is now invalid.", TRUE);
}

void unload_latest()
{
  INTVECT p_vect;
  V3_CDS_PTR cds_ptr;
  SIGREC_PTR sig_ptr;
  uint psp;

  // Note that we step backwards to allow unloading of Multiple copies
  // in reverse order to loading, so that the Int 2Fh chain remains
  // intact.
  for (ul_i = 0x66; ul_i >= 0x60; ul_i--) {
    long far *p;

    p = (long far *) MK_FP(0, ((uint) ul_i * 4));
    sig_ptr = (SIGREC_PTR) * p;
    if (_fmemcmp(sig_ptr->signature, (uchar far *) sigrec.signature,
                 sizeof(sigrec.signature)) == 0)
      break;
  }

  if (ul_i == 0x5f)
    failprog("Phantom not loaded.");

  p_vect = _dos_getvect(0x2f);

  // Check that a subsequent TSR hasn't taken over Int 2Fh
  if (sig_ptr->our_handler != (void far *) p_vect)
    failprog("Interrupt 2F has been superceded...");

  p_vect = (INTVECT) sig_ptr->prev_handler;
  _dos_setvect(0x2f, p_vect);
  p_vect = _dos_getvect(ul_i);
  psp = ((SIGREC_PTR) p_vect)->psp;
  our_drive_no = ((SIGREC_PTR) p_vect)->drive_no;

  // Free up the XMS memory
  if ((!xms_is_present()) || (!xms_free_block(((SIGREC_PTR) p_vect)->xms_handle)))
    print_string("Could not free XMS memory", TRUE);

  cds_ptr = lolptr->cds_ptr;
  if (_osmajor == 3)
//              cds_ptr += (our_drive_no - 1);  // ref: DR_TOO_HIGH
    cds_ptr += our_drive_no;
  else {
    V4_CDS_PTR t = (V4_CDS_PTR) cds_ptr;

//              t += (our_drive_no - 1);  // ref: DR_TOO_HIGH
    t += our_drive_no;
    cds_ptr = (V3_CDS_PTR) t;
  }

  // switch off the Network and Physical bits for the drive,
  // rendering it invalid.
  cds_ptr->flags = cds_ptr->flags & 0x3fff;

  // Use the recommended switch PSP and Int 4Ch method of
  // unloading the TSR (see TSRs chapter of Undocumented DOS).
  _asm {
    // Save some registers
    push es;
    push ds;
    push si;
    push di;
    push bp;

    // Set resident program's parent PSP to us.
    mov es, psp;
    mov bx, 0x16;
    mov ax, _psp;
    mov es:[di], ax;
    mov di, 0x0a;

    // Set resident program PSP return address to exit_ret;
    mov ax, offset exit_ret;

    stosw;
    mov ax, cs;

    stosw;
    mov bx, es;

    // Set current PSP to resident program
    mov ah, 0x50;
    int 0x21;

    // Save SS:SP
    mov ax, seg ul_save_ss;
    mov ds, ax;
    mov ul_save_ss, ss;
    mov ul_save_sp, sp;

    // and terminate
    mov ax, 0x4c00;
    int 0x21;
  }
}

/* ------- TSR termination routines -------- */

/* Plug into Int 2Fh, and calculate the size of the TSR to
        keep in memory. Plug into a 'user' interrupt to allow for
        unloading */

void prepare_for_tsr(void)
{
  uchar far *buf;
  int i;

  // Find ourselves a free interrupt to call our own. Without it,
  // we can still load, but a future invocation of Phantom with -U
  // will not be able to unload us.
  for (i = 0x60; i < 0x67; i++) {
    long far *p;

    p = (long far *) MK_FP(0, ((uint) i * 4));
    if (*p == 0L)
      break;
  }

  prev_int2f_vector = _dos_getvect(0x2f);
  if (i == 0x67) {
    print_string("No user intrs available. Phantom not unloadable..", TRUE);
    return;
  }

  // Our new found 'user' interrupt will point at the command line area of
  // our PSP. Complete our signature record, put it into the command line,
  // then go to sleep.

  _dos_setvect(i, (INTVECT) (buf = MK_FP(_psp, 0x80)));

  sigrec.xms_handle = xms_handle;
  sigrec.psp = _psp;
  sigrec.drive_no = our_drive_no;
  sigrec.our_handler = (void far *) redirector;
  sigrec.prev_handler = (void far *) prev_int2f_vector;
  *((SIGREC_PTR) buf) = sigrec;
}

void tsr(void)
{
  uint tsr_paras;               // Paragraphs to terminate and leave resident.
  uint highest_seg;

  _asm mov highest_seg, ds;

  tsr_paras = highest_seg + (((uint) &end) / 16) + 1 - _psp;
  consolef("PARAS: %i\n", tsr_paras);

  // Plug ourselves into the Int 2Fh chain
  _dos_setvect(0x2f, redirector);
  _dos_keep(0, tsr_paras);
}

/* --------------------------------------------------------------------*/

int _cdecl main(uint argc, char **argv)
{
  print_string(signon_string, TRUE);

  // See what parameters we have...
  for (argv++; *argv; argv++) {
    switch (**argv) {
    case '-':
    case '/':
      (*argv)++;
      switch (toupper(**argv)) {
      case 'U':
        get_dos_vars();
        unload_latest();
        return 0;
      case 'S':
        (*argv)++;
        if (!(disk_size = atoi(*argv))) {
          print_string("Bad size parameter.", TRUE);
          return 0;
        }
        break;
      default:
        print_string("Unrecognized parameter.", TRUE);
        return 0;
      }
      break;
    default:
      our_drive_str[0] = **argv;
    }
  }

  // Otherwise, check that it's a valid drive letter
  if (our_drive_str[0] == ' ') {
    print_string(usage_string, TRUE);
    return 0;
  }

  our_drive_str[0] &= ~0x20;
//      our_drive_no = (uchar) (our_drive_str[0] - '@');  // ref: DR_TOO_HIGH
  our_drive_no = (uchar) (our_drive_str[0] - 'A');
//      if ((our_drive_no > 26) || (our_drive_no < 1))  // ref: DR_TOO_HIGH
  if (our_drive_no > 25) {
    print_string(usage_string, TRUE);
    return 0;
  }

  // Initialize XMS and alloc the 'disk space'
  set_up_xms_disk();
  is_ok_to_load();
  get_dos_vars();
  set_up_cds();
  set_up_pointers();

  // Tell the user
  print_string(my_ltoa(disk_size), FALSE);
  print_string("Kb XMS allocated.", TRUE);
  print_string("FujiNet installed as ", FALSE);
  print_string(our_drive_str, TRUE);

  prepare_for_tsr();

  tsr();

  return 0;
}
