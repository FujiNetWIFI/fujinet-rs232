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
#include "fujifs.h"
#include <stdio.h>	// printf
#include <stdlib.h>
#include <dos.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <bios.h>
#include <time.h>
#include <conio.h>	// getch
#include <strings.h>	// strcasecmp

#define DEBUG
#ifdef DEBUG
#include "../../fujinet-rs232/sys/print.h"
#endif

/* ****************************************************
   Constants and Macros
   **************************************************** */

#define         ROOTDIR_ENTRIES         128

#ifndef MK_FP
#define MK_FP(a,b)  ((void far *)(((uint32_t)(a) << 16) | (b)))
#endif

/* ************************************************
   Global data declarations
   ************************************************ */

/* This is declared in the compiler startup code to mark the
        end of the data segment. */
extern uint16_t end;

/* TSR signature and unload info structure */
typedef struct {
  uint8_t cmdline_len;
  char signature[10];           /* The TSR's signature string */
  uint16_t psp;                     /* This instance's PSP */
  uint8_t drive_no;               /* A: is 1, B: is 2, etc. */
  uint16_t xms_handle;              /* This instance's disk XMS handle */
  uint8_t far *our_handler;       /* This instance's int 2Fh handler */
  uint8_t far *prev_handler;      /* Previous int 2Fh handler in the chain */
} SIGREC, far *SIGREC_PTR;

/* Other global data items */
SIGREC sigrec = { 8, "PHANTOM ", 0, 0, 0 };     /* Signature record */
LOLREC_PTR lolptr;              /* pointer to List Of Lists */

/* ------ File system functions ------------------------ */

/* Fail Phantom, print message, exit to DOS */
void failprog(char *msg)
{
  if (xms_handle)
    xms_free_block(xms_handle);
  print_string((uint8_t far *) msg, TRUE);
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
    dirrec_ptr1 = &((SDA_PTR_V3) sda_ptr)->dirrec;
    dirrec_ptr2 = &((SDA_PTR_V3) sda_ptr)->rename_dirrec;
    fcbname_ptr1 = ((SDA_PTR_V3) sda_ptr)->fcb_name1;
    fcbname_ptr2 = ((SDA_PTR_V3) sda_ptr)->fcb_name2;
    filename_ptr1 = ((SDA_PTR_V3) sda_ptr)->path1 + cds_root_size - 1;
    filename_ptr2 = ((SDA_PTR_V3) sda_ptr)->path2 + cds_root_size - 1;
    srch_attr_ptr = &((SDA_PTR_V3) sda_ptr)->srch_attr;
    srchrec_ptr1 = &((SDA_PTR_V3) sda_ptr)->srchrec;
    srchrec_ptr2 = &((SDA_PTR_V3) sda_ptr)->rename_srchrec;
  }
  else {
    dirrec_ptr1 = &((SDA_PTR_V4) sda_ptr)->dirrec;
    dirrec_ptr2 = &((SDA_PTR_V4) sda_ptr)->rename_dirrec;
    fcbname_ptr1 = ((SDA_PTR_V4) sda_ptr)->fcb_name1;
    fcbname_ptr2 = ((SDA_PTR_V4) sda_ptr)->fcb_name2;
    filename_ptr1 = ((SDA_PTR_V4) sda_ptr)->path1 + cds_root_size - 1;
    filename_ptr2 = ((SDA_PTR_V4) sda_ptr)->path2 + cds_root_size - 1;
    srch_attr_ptr = &((SDA_PTR_V4) sda_ptr)->srch_attr;
    srchrec_ptr1 = &((SDA_PTR_V4) sda_ptr)->srchrec;
    srchrec_ptr2 = &((SDA_PTR_V4) sda_ptr)->rename_srchrec;
  }
}

/* Get DOS version, address of Swappable DOS Area, and address of
        DOS List of lists. We only run on versions of DOS >= 3.10, so
        fail otherwise */

void get_dos_vars(void)
{
  uint16_t segmnt;
  uint16_t ofset;

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
  CDS_PTR_V3 our_cds_ptr;

  our_cds_ptr = lolptr->cds_ptr;
  if (_osmajor == 3)
//              our_cds_ptr = our_cds_ptr + (fn_drive_num - 1);  // ref: DR_TOO_HIGH
    our_cds_ptr = our_cds_ptr + fn_drive_num;
  else {
    CDS_PTR_V4 t = (CDS_PTR_V4) our_cds_ptr;

//              t = t + (fn_drive_num - 1);  // ref: DR_TOO_HIGH
    t = t + fn_drive_num;
    our_cds_ptr = (CDS_PTR_V3) t;
  }

//      if (fn_drive_num > lolptr->last_drive)  // ref: DR_TOO_HIGH
  if (fn_drive_num >= lolptr->last_drive)
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
//              (char) ('@'+ fn_drive_num);  // ref: DR_TOO_HIGH
    (char) ('A' + fn_drive_num);
  _fstrcpy(cds_path_root, our_cds_ptr->current_path);
  current_path = our_cds_ptr->current_path;
  our_cds_ptr->root_ofs = _fstrlen(our_cds_ptr->current_path) - 1;
  current_path += our_cds_ptr->root_ofs;
}

/* ---- Unload functionality --------------*/

/* Find the latest Phantom installed, unplug it from the Int 2F
        chain if possible, make the CDS reflect an invalid drive, and
        free its real and XMS memory. */

static uint16_t ul_save_ss, ul_save_sp;
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
  printf("%c is now invalid.\n", fn_drive_num + 'A');
  return;
}

void unload_latest()
{
  INTVECT p_vect;
  CDS_PTR_V3 cds_ptr;
  SIGREC_PTR sig_ptr;
  uint16_t psp;

  // Note that we step backwards to allow unloading of Multiple copies
  // in reverse order to loading, so that the Int 2Fh chain remains
  // intact.
  for (ul_i = 0x66; ul_i >= 0x60; ul_i--) {
    long far *p;

    p = (long far *) MK_FP(0, ((uint16_t) ul_i * 4));
    sig_ptr = (SIGREC_PTR) * p;
    if (_fmemcmp(sig_ptr->signature, (uint8_t far *) sigrec.signature,
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
  fn_drive_num = ((SIGREC_PTR) p_vect)->drive_no;

  // Free up the XMS memory
  if ((!xms_is_present()) || (!xms_free_block(((SIGREC_PTR) p_vect)->xms_handle)))
    print_string("Could not free XMS memory", TRUE);

  cds_ptr = lolptr->cds_ptr;
  if (_osmajor == 3)
//              cds_ptr += (fn_drive_num - 1);  // ref: DR_TOO_HIGH
    cds_ptr += fn_drive_num;
  else {
    CDS_PTR_V4 t = (CDS_PTR_V4) cds_ptr;

//              t += (fn_drive_num - 1);  // ref: DR_TOO_HIGH
    t += fn_drive_num;
    cds_ptr = (CDS_PTR_V3) t;
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
  uint8_t far *buf;
  int i;

  // Find ourselves a free interrupt to call our own. Without it,
  // we can still load, but a future invocation of Phantom with -U
  // will not be able to unload us.
  for (i = 0x60; i < 0x67; i++) {
    long far *p;

    p = (long far *) MK_FP(0, ((uint16_t) i * 4));
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
  sigrec.drive_no = fn_drive_num;
  sigrec.our_handler = (void far *) redirector;
  sigrec.prev_handler = (void far *) prev_int2f_vector;
  *((SIGREC_PTR) buf) = sigrec;
}

void tsr(void)
{
  uint16_t tsr_paras;               // Paragraphs to terminate and leave resident.
  uint16_t highest_seg;

  _asm mov highest_seg, ds;

  tsr_paras = highest_seg + (((uint16_t) &end) / 16) + 1 - _psp;
  consolef("PARAS: %i\n", tsr_paras);

  // Plug ourselves into the Int 2Fh chain
  _dos_setvect(0x2f, redirector);
  _dos_keep(0, tsr_paras);
}

/* --------------------------------------------------------------------*/

void get_password(char *password, size_t max_len)
{
  size_t idx = 0;
  char ch;


  while (idx < max_len - 1) {
    ch = getch();

    if (ch == '\r' || ch == '\n')
      break;

    // Handle backspace/delete
    if (ch == '\b' || ch == 127) {
      if (idx) {
        // Erase '*' from the screen
        printf("\b \b");
        fflush(stdout);
        idx--;
      }
    }
    else {
      password[idx++] = ch;
      printf("*");
      fflush(stdout);
    }
  }

  password[idx] = 0;
  printf("\n");
  return;
}

int _cdecl main(uint16_t argc, char **argv)
{
  uint8_t drive_letter;
  const char *url;
  errcode err;
  

  if (argc < 2) {
    printf("Usage: %s <command>\n", argv[0]);
    exit(1);
  }

  // Only support map command at this time
  if (strcasecmp(argv[1], "map") != 0 || argc < 4) {
    printf("Usage: %s map L: <url_of_share>\n", argv[0]);
    exit(1);
  }

  drive_letter = toupper(argv[2][0]);
  fn_drive_num = drive_letter - 'A';
  url = argv[3];

  err = fujifs_open_url(&fn_host, url, NULL, NULL);
  if (err) {
    // Maybe authentication is needed?
    printf("User: ");
    fgets(sector_buffer, 128, stdin);
    if (sector_buffer[0])
      sector_buffer[strlen(sector_buffer) - 1] = 0;
    printf("Password: ");
    fflush(stdout);
    get_password(&sector_buffer[128], 128);

    err = fujifs_open_url(&fn_host, url, sector_buffer, &sector_buffer[128]);
    if (err) {
      printf("Err: %i unable to open URL: %s\n", err, url);
      exit(1);
    }
  }

  // Opened succesfully, we don't need it anymore
  err = fujifs_close_url(fn_host);

  strcpy(fn_volume, url);
  end = strlen(fn_volume);
  if (fn_volume[end - 1] == '/')
    fn_volume[end - 1] = 0;
  fn_cwd[0] = 0;

  // Initialize XMS and alloc the 'disk space'
  set_up_xms_disk();
  is_ok_to_load();
  get_dos_vars();
  set_up_cds();
  set_up_pointers();

  // Tell the user
  printf("FujiNet installed as %c:\n", fn_drive_num + 'A');

  prepare_for_tsr();

  tsr();

  return 0;
}
