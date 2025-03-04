#include "redir.h"
#include "ramdrive.h"
#include "dosfunc.h"
#include "../ncopy/fujifs.h"
#include "../fnshare/doserr.h"
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <ctype.h>

#define DO_FUJI 1
#define DEBUG
#define DEBUG_DISPATCH
#if defined(DEBUG)
#include "../../fujinet-rs232/sys/print.h"
#endif

#define URL "tnfs://10.4.0.1"
#define FNDIR_HANDLE dir_sector
#define FNSFT_HANDLE start_sector

#define STACK_SIZE 1024

/* all the calls we need to support are in the range 0..2Eh */
/* This serves as a list of the function types that we support */
#define _inquiry        0x00
#define _rd             0x01
#define _md             0x03
#define _cd             0x05
#define _clsfil         0x06
#define _cmmtfil        0x07
#define _readfil        0x08
#define _writfil        0x09
#define _lockfil        0x0A
#define _unlockfil      0x0B
#define _dskspc         0x0C
#define _setfatt        0x0E
#define _getfatt        0x0F
#define _renfil         0x11
#define _delfil         0x13
#define _opnfil         0x16
#define _creatfil       0x17
#define _ffirst         0x1B
#define _fnext          0x1C
#define _skfmend        0x21
#define _unknown_fxn_2D 0x2D
#define _spopnfil       0x2E
#define _unsupported    0xFF

// FIXME - this is essentially the same as dispatch_table
uchar fxnmap[] = {
  _inquiry,     /* 0x00h */
  _rd,  /* 0x01h */
  _unsupported, /* 0x02h */
  _md,  /* 0x03h */
  _unsupported, /* 0x04h */
  _cd,  /* 0x05h */
  _clsfil,      /* 0x06h */
  _cmmtfil,     /* 0x07h */
  _readfil,     /* 0x08h */
  _writfil,     /* 0x09h */
  _lockfil,     /* 0x0Ah */
  _unlockfil,   /* 0x0Bh */
  _dskspc,      /* 0x0Ch */
  _unsupported, /* 0x0Dh */
  _setfatt,     /* 0x0Eh */
  _getfatt,     /* 0x0Fh */
  _unsupported, /* 0x10h */
  _renfil,      /* 0x11h */
  _unsupported, /* 0x12h */
  _delfil,      /* 0x13h */
  _unsupported, /* 0x14h */
  _unsupported, /* 0x15h */
  _opnfil,      /* 0x16h */
  _creatfil,    /* 0x17h */
  _unsupported, /* 0x18h */
  _unsupported, /* 0x19h */
  _unsupported, /* 0x1Ah */
  _ffirst,      /* 0x1Bh */
  _fnext,       /* 0x1Ch */
  _unsupported, /* 0x1Dh */
  _unsupported, /* 0x1Eh */
  _unsupported, /* 0x1Fh */
  _unsupported, /* 0x20h */
  _skfmend,     /* 0x21h */
  _unsupported, /* 0x22h */
  _unsupported, /* 0x23h */
  _unsupported, /* 0x24h */
  _unsupported, /* 0x25h */
  _unsupported, /* 0x26h */
  _unsupported, /* 0x27h */
  _unsupported, /* 0x28h */
  _unsupported, /* 0x29h */
  _unsupported, /* 0x2Ah */
  _unsupported, /* 0x2Bh */
  _unsupported, /* 0x2Ch */
  _unknown_fxn_2D,      /* 0x2Dh */
  _spopnfil     /* 0x2Eh */
};

ALL_REGS r;                     /* Global save area for all caller's regs */
uchar our_drive_no;             /* A: is 1, B: is 2, etc. */
char our_drive_str[3] = " :";   /* Our drive letter string */
char far *cds_path_root = "Phantom  :\\";       /* Root string for CDS */
uint cds_root_size;             /* Size of our CDS root string */
uint far *stack_param_ptr;      /* ptr to word at top of stack on entry */
int curr_fxn;                   /* Record of function in progress */
int filename_is_char_device;    /* generate_fcbname found character device name */
INTVECT prev_int2f_vector;      /* For chaining, and restoring on unload */

uint dos_ss;                    /* DOS's saved SS at entry */
uint dos_sp;                    /* DOS's saved SP at entry */
uint our_sp;                    /* SP to switch to on entry */
uint save_sp;                   /* SP saved across internal DOS calls */
char our_stack[STACK_SIZE];     /* our internal stack */

/* these are version independent pointers to various frequently used
        locations within the various DOS structures */
uchar far *sda_ptr;             /* ptr to SDA */
char far *current_path;         /* ptr to current path in CDS */
char far *filename_ptr;         /* ptr to 1st filename area in SDA */
char far *filename_ptr_2;       /* ptr to 2nd filename area in SDA */
char far *fcbname_ptr;          /* ptr to 1st FCB-style name in SDA */
char far *fcbname_ptr_2;        /* ptr to 2nd FCB-style name in SDA */
uchar far *srch_attr_ptr;       /* ptr to search attribute in SDA */
SRCHREC_PTR srchrec_ptr;        /* ptr to 1st Search Data Block in SDA */
SRCHREC_PTR srchrec_ptr_2;      /* ptr to 2nd Search Data Block in SDA */
DIRREC_PTR dirrec_ptr;          /* ptr to 1st found dir entry area in SDA */
DIRREC_PTR dirrec_ptr_2;        /* ptr to 1st found dir entry area in SDA */

#ifdef __WATCOMC__
#define FCARRY				INTR_CF
#else
#define FCARRY                          0x0001
#endif

extern __segment getSP(void);
#pragma aux getSP = \
    "mov ax, sp";
extern __segment getSS(void);
#pragma aux getSS = \
    "mov ax, ss";
extern __segment getDS(void);
#pragma aux getDS = \
    "mov ax, ds";

/* Fail the current redirector call with the supplied error number, i.e.
   set the carry flag in the returned flags, and set ax=error code */

void fail(uint err)
{
  r.flags = (r.flags | FCARRY);
  r.ax = err;
}

/* Opposite of fail() ! */

void succeed(void)
{
  r.flags = (r.flags & ~FCARRY);
  r.ax = 0;
}

/* Does the supplied string contain a wildcard '?' */
int contains_wildcards(char far *path)
{
  int i;

  for (i = 0; i < 11; i++)
    if (path[i] == '?')
      return TRUE;
  return FALSE;
}

/* ----- Redirector functions ------------------*/

/* Respond that it is OK to load another redirector */
void inquiry(void)
{
  r.ax = 0x00FF;
}

int filename_match(char far *pattern, char far *filename)
{
  int idx, jdx;


#ifdef DEBUG
  //consolef("FILENAME_MATCH \"%ls\" \"%ls\"\n", pattern, filename);
#endif
  // Compare the name part (first 8 characters in DOS)
  for (idx = 0; idx < 8; idx++) {
    if (pattern[idx] == ' ')
      break; // End of name in pattern

    if (pattern[idx] == '?') {
      if (!filename[idx] || filename[idx] == '.')
        break; // '?' matches even if filename is shorter
      continue;
    }

    if (pattern[idx] != toupper(filename[idx]))
      return 0;
  }

  // If disk name still has characters before '.', mismatch
  if (filename[idx] && filename[idx] != '.')
    return 0;

  // Skip '.' in disk name if present
  if (filename[idx] == '.')
    idx++;
  jdx = idx;

  // Compare the extension
  for (idx = 8; idx < 11; idx++, jdx++) {
    if (pattern[idx] == ' ')
      break; // End of extension in pattern

    if (pattern[idx] == '?') {
      if (!filename[jdx])
        break; // '?' matches even if extension is shorter
      continue;
    }

    if (pattern[idx] != toupper(filename[jdx]))
      return 0;
  }

  return !filename[jdx];
}

static char *fake_files[] = {
  "FILE1.DAT", "ANOTHER.HI", "TESTING.123", "LONGNAME.TXT", NULL,
};
static FN_DIRENT fake_ent;

void fcbitize(char far *dest, const char *source)
{
  const char *dot, *ext;
  int len;


  _fmemset(dest, ' ', 11);
  dot = strchr(source, '.');
  if (dot)
    ext = dot + 1;
  else {
    dot = source + strlen(source);
    ext = NULL;
  }
  len = dot - source;
  _fmemcpy(dest, source, len <= 8 ? len : 8);
  if (ext) {
    len = strlen(ext);
    _fmemcpy(&dest[8], ext, len <= 3 ? len : 3);
  }

  return;
}

/* FindNext  - subfunction 1Ch */
void fnext(void)
{
#if 0
  if (!find_next_entry(srchrec_ptr->srch_mask,
                       srchrec_ptr->attr_mask, dirrec_ptr->file_name,
                       &dirrec_ptr->file_attr, &dirrec_ptr->file_time,
                       &dirrec_ptr->start_sector, &dirrec_ptr->file_size,
                       &srchrec_ptr->dir_sector, &srchrec_ptr->dir_entry_no)) {
    fail(18);
    return;
  }
#else
  FN_DIRENT *ent;
  uint8_t search_attr;
  uint16_t dos_date, dos_time;


  // FIXME - make these arguments instead of accessing globals
  search_attr = srchrec_ptr->attr_mask;

  consolef("FNEXT \"%ls\"\n", srchrec_ptr->srch_mask);
  //consolef("DIR ENTRY NO %i\n", srchrec_ptr->dir_entry_no);
  if (srchrec_ptr->FNDIR_HANDLE == -1) {
#if DO_FUJI
    fujifs_handle handle;
    errcode err;


    err = fujifs_opendir(0, &handle, URL);
    if (err) {
      fail(DOSERR_UNEXPECTED_NETWORK_ERROR);
      return;
    }
    srchrec_ptr->FNDIR_HANDLE = handle;
#else
    srchrec_ptr->FUJINET_HANDLE = 0;
#endif
    srchrec_ptr->dir_entry_no = 0;
  }

  while (1) {
#if DO_FUJI
    ent = fujifs_readdir(srchrec_ptr->FNDIR_HANDLE);
#else
    ent = NULL;
    if (fake_files[srchrec_ptr->FUJINET_HANDLE]) {
      ent = &fake_ent;
      fake_ent.name = fake_files[srchrec_ptr->FUJINET_HANDLE++];
      fake_ent.mtime.tm_mon = srchrec_ptr->FUJINET_HANDLE;
      fake_ent.mtime.tm_year = 2025 - srchrec_ptr->FUJINET_HANDLE - 1900;
      fake_ent.size = 17 * srchrec_ptr->FUJINET_HANDLE;
    }
#endif
#ifdef DEBUG
    //consolef("ENT: 0x%04x 0x%04x\n", ent, ent->name);
#endif
    if (!ent) {
#if defined(DEBUG)
      consolef("OUT OF ENTRIES %i\n", srchrec_ptr->FNDIR_HANDLE);
#endif
#if DO_FUJI
      fujifs_closedir(srchrec_ptr->FNDIR_HANDLE);
#endif
      srchrec_ptr->FNDIR_HANDLE = -1;
      fail(DOSERR_NO_MORE_FILES);
      return;
    }
    if (filename_match(fcbname_ptr, ent->name))
      break;
  }

#if 0
  if (!contains_wildcards(fcbname_ptr)) {
#if DO_FUJI
    fujifs_closedir(srchrec_ptr->FUJINET_HANDLE);
#endif
    srchrec_ptr->FUJINET_HANDLE = -1;
  }
#endif

  fcbitize(dirrec_ptr->file_name, ent->name);
  dirrec_ptr->file_attr = ent->isdir ? 0x10/*ATTR_DIRECTORY*/ : 0;
  dos_date = (ent->mtime.tm_mday)
    | ((ent->mtime.tm_mon + 1) << 5) | ((ent->mtime.tm_year - 80) << 9);
  dos_time = (ent->mtime.tm_sec / 2)
    | (ent->mtime.tm_min << 5) | (ent->mtime.tm_hour << 11);
  dirrec_ptr->file_time = (ulong) MK_FP(dos_date, dos_time);
  dirrec_ptr->file_size = ent->size;

  // Does DOS actually look at these fields?
  dirrec_ptr->start_sector = 1;

#endif
}

/* Internal findnext for delete and rename processing */
uint fnext2(void)
{
  return (find_next_entry(srchrec_ptr_2->srch_mask, 0x20,
                          dirrec_ptr_2->file_name, &dirrec_ptr_2->file_attr,
                          NULL, NULL, NULL, &srchrec_ptr_2->dir_sector,
                          &srchrec_ptr_2->dir_entry_no)) ? 0 : 18;
}

/* FindFirst - subfunction 1Bh */

/* This function looks a little odd because of the embedded call to
   fnext(). This arises from the my view that findfirst is simply
   a findnext with some initialization overhead: findfirst has to
   locate the directory in which findnext is to iterate, and
   initialize the SDB state to 'point to' the first entry. It then
   gets that first entry, using findnext.
   The r.ax test at the end of the function is because, to mimic
   DOS behavior, a findfirst that finds no matching entry should
   return an error 2 (file not found), whereas a subsequent findnext
   that finds no matching entry should return error 18 (no more
   files). */

void ffirst(void)
{
  char far *path;
  int success;

  consolef("FFIRST 0x%02x\n", *srch_attr_ptr);
#if 0
  /* Special case for volume-label-only search - must be in root */
  if (path = (*srch_attr_ptr == 0x08)
      ? filename_ptr : _fstrrchr(filename_ptr, '\\'))
    *path = 0;
  consolef("SRCH ATTR PTR 0x%02x\n", *srch_attr_ptr);
  success = get_dir_start_sector(filename_ptr, &srchrec_ptr->dir_sector);
  if (path)
    *path = '\\';
  if (!success) {
    fail(3);
    return;
  }
#else
  if ((*srch_attr_ptr) & 0x08/*ATTR_VOLUME_LABEL*/) {
    //consolef("VOLUME\n");
    srchrec_ptr->drive_no = (our_drive_no + 1) | 0x80;
    _fmemmove(srchrec_ptr->srch_mask, fcbname_ptr, sizeof(srchrec_ptr->srch_mask));
    srchrec_ptr->attr_mask = *srch_attr_ptr;
    _fstrcpy(dirrec_ptr->file_name, "FUJINET1234");
    dirrec_ptr->file_attr = 0x08/*ATTR_VOLUME_LABEL*/;
    dirrec_ptr->file_time = 0;
    dirrec_ptr->file_size = 0;

    //dumpHex(search, sizeof(*search), 0);
    //dumpHex(dirrec_ptr, sizeof(*dirrec_ptr), 0);
    succeed();
    consolef("FOUND VOLUME\n");
    return;
  }
#endif

  _fmemcpy(&srchrec_ptr->srch_mask, fcbname_ptr, 11);

  if (srchrec_ptr->FNDIR_HANDLE != -1) {
#if DO_FUJI
    fujifs_closedir(srchrec_ptr->FNDIR_HANDLE);
#endif
    srchrec_ptr->FNDIR_HANDLE = -1;
  }
  srchrec_ptr->dir_entry_no = -1;
  srchrec_ptr->attr_mask = *srch_attr_ptr;
  srchrec_ptr->drive_no = (uchar) (our_drive_no | 0xC0);

  fnext();
#ifdef DEBUG
  //consolef("FNEXT %i\n", r.ax);
#endif
  /* No need to check r.flags & FCARRY; if ax is 18,
     FCARRY must have been set. */
  if (r.ax == 18)
    r.ax = 2;   // make fnext error code suitable to ffirst
}

/* Internal findfirst for delete and rename processing */
uint ffirst2(void)
{
  if (!get_dir_start_sector(filename_ptr_2, &srchrec_ptr_2->dir_sector))
    return 3;

  srchrec_ptr_2->dir_entry_no = -1;
  srchrec_ptr_2->drive_no = (uchar) (our_drive_no | 0x80);

  return fnext2();
}

/* ReMove Directory - subfunction 01h */
void rd(void)
{
  /* special case for root */
  if ((*filename_ptr == '\\') && (!*(filename_ptr + 1))) {
    fail(5);
    return;
  }
  if (contains_wildcards(fcbname_ptr)) {
    fail(3);
    return;
  }
  _fstrcpy(filename_ptr_2, filename_ptr);
  *srch_attr_ptr = 0x10;

  ffirst();
  if (r.ax || (!(dirrec_ptr->file_attr & 0x10))) {
    r.ax = 3;
    return;
  }

  if (!_fstrncmp(filename_ptr_2, current_path, _fstrlen(filename_ptr_2))) {
    fail(16);
    return;
  }

  _fmemset(srchrec_ptr_2->srch_mask, '?', 11);
  srchrec_ptr_2->attr_mask = 0x3f;

  if ((r.ax = ffirst2()) == 3) {
    fail(3);
    return;
  }

  if (!r.ax) {
    fail(5);
    return;
  }

  if (!get_sector(last_sector = srchrec_ptr->dir_sector, sector_buffer)) {
    fail(5);
    return;
  }
  ((DIRREC_PTR) sector_buffer)[srchrec_ptr->dir_entry_no].file_name[0] = (char) 0xE5;
  if (  /* dirsector_has_entries(last_sector, sector_buffer) && */
       (!put_sector(last_sector, sector_buffer))) {
    fail(5);
    return;
  }

  FREE_SECTOR_CHAIN(dirrec_ptr->start_sector);
  succeed();
}

/* Make Directory - subfunction 03h */
void md(void)
{
  /* special case for root */
  if ((*filename_ptr == '\\') && (!*(filename_ptr + 1))) {
    fail(5);
    return;
  }
  // can't create dir name with * or ? in it
  if (contains_wildcards(fcbname_ptr)) {
    fail(3);
    return;
  }

  *srch_attr_ptr = 0x3f;
  ffirst();
  if (r.ax == 0)        // we need error 2 here
  {
    fail(5);
    return;
  }
  if (r.ax != 2)
    return;

  /*
     Note that although we initialize a directory sector, we actually
     needn't, since we do not create . or .. entries. This is because
     a redirector never receives requests for . or .. in ChDir - the
     absolute path is resolved by DOS before we get it. If you want
     to see dots in DIR listings, create directory entries for them
     after put_sectors. Note that you will also have to take account
     of them in RMDIR.
   */
  last_sector = 0xffff;
  memset(sector_buffer, 0, SECTOR_SIZE);
  if (!(dirrec_ptr->start_sector = next_free_sector())) {
    fail(5);
    return;
  }
  set_next_sector(dirrec_ptr->start_sector, 0xFFFF);
  last_sector = dirrec_ptr->start_sector;
  if ((!put_sector(dirrec_ptr->start_sector, sector_buffer)) ||
      (!create_dir_entry(&srchrec_ptr->dir_sector, NULL, fcbname_ptr, 0x10,
                         dirrec_ptr->start_sector, 0, dos_ftime()))) {
    fail(5);
    return;
  }
  succeed();
}

/* Change Directory - subfunction 05h */
void cd(void)
{
  fail(3);
  return;
  /* Special case for root */
  if ((*filename_ptr != '\\') || (*(filename_ptr + 1))) {
    if (contains_wildcards(fcbname_ptr)) {
      fail(3);
      return;
    }

    *srch_attr_ptr = 0x10;
    ffirst();
    if (r.ax || (!(dirrec_ptr->file_attr & 0x10))) {
      fail(3);
      return;
    }
  }
  _fstrcpy(current_path, filename_ptr);
}

/* Close File - subfunction 06h */
void clsfil(void)
{
  SFTREC_PTR p = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (p->handle_count)  /* If handle count not 0, decrement it */
    --p->handle_count;

#if 0
  /* If writing, create/update dir entry for file */
  if (!(p->open_mode & 3))
    return;

  if (p->dir_entry_no == 0xff) {
    if (!create_dir_entry(&p->dir_sector, &p->dir_entry_no, p->file_name,
                          p->file_attr, p->start_sector, p->file_size, p->file_time))
      fail(5);
  }
  else {
    if ((last_sector != p->dir_sector) && (!get_sector(p->dir_sector, sector_buffer)))
      fail(5);
    last_sector = p->dir_sector;
    ((DIRREC_PTR) sector_buffer)[p->dir_entry_no].file_attr = p->file_attr;
    ((DIRREC_PTR) sector_buffer)[p->dir_entry_no].start_sector = p->start_sector;
    ((DIRREC_PTR) sector_buffer)[p->dir_entry_no].file_size = p->file_size;
    ((DIRREC_PTR) sector_buffer)[p->dir_entry_no].file_time = p->file_time;
    if (!put_sector(p->dir_sector, sector_buffer))
      fail(5);
  }
#else
  fujifs_close(p->FNSFT_HANDLE);
#endif  
}

/* Commit File - subfunction 07h */
void cmmtfil(void)
{
  /* We support this but don't do anything... */
  return;
}

/* Read from File - subfunction 08h */
// For version that handles critical errors,
// see Undocumented DOS, 2nd edition, chapter 8
void readfil(void)
{
  SFTREC_PTR p = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (p->open_mode & 1) {
    fail(5);
    return;
  }

#if 0
  if ((p->file_pos + r.cx) > p->file_size)
    r.cx = (uint) (p->file_size - p->file_pos);

  if (!r.cx)
    return;

  /* Fill caller's buffer and update the SFT for the file */
  read_data(&p->file_pos, &r.cx, ((V3_SDA_PTR) sda_ptr)->current_dta,
            p->start_sector, &p->rel_sector, &p->abs_sector);
#else
#if DO_FUJI
  consolef("REQUESTING %i from %i\n", r.cx, p->FNSFT_HANDLE);
  r.cx = fujifs_read(p->FNSFT_HANDLE, ((V3_SDA_PTR) sda_ptr)->current_dta, r.cx);
#else
  if (p->file_pos >= r.cx)
    r.cx = 20;
#endif
  p->file_pos += r.cx;
#endif
#if 0 && defined(DEBUG)
  consolef("SFT: %i\n", r.cx);
  dumpHex(p, sizeof(*p), 0);
  consolef("REGS:\n");
  dumpHex(&r, sizeof(r), 0);
#endif
}

/* Write to File - subfunction 09h */
void writfil(void)
{
  SFTREC_PTR p = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (!(p->open_mode & 3)) {
    fail(5);
    return;
  }

  p->file_time = dos_ftime();

  /* Take account of DOS' 0-byte-write-truncates-file rcounte */
  if (!r.cx) {
    p->file_size = p->file_pos;
    chop_file(p->file_pos, &p->start_sector, &p->rel_sector, &p->abs_sector);
    return;
  }

  /* Write from the caller's buffer and update the SFT for the file */
  write_data(&p->file_pos, &r.cx, ((V3_SDA_PTR) sda_ptr)->current_dta,
             &p->start_sector, &p->rel_sector, &p->abs_sector);
  if (p->file_pos > p->file_size)
    p->file_size = p->file_pos;
}

/* Lock file - subfunction 0Ah */

/* We support this function only to illustrate how it works. We do
        not actually honor LOCK/UNLOCK requests. The following function
        supports locking only before, and both locking/unlocking after
        DOS 4.0 */
void lockfil(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  LOCKREC_PTR lockptr;
  ulong region_offset;
  ulong region_length;

  if (_osmajor > 3) {
    // In v4.0 and above, lock info is at DS:BX in a LOCKREC structure
    lockptr = (LOCKREC_PTR) MK_FP(r.ds, r.dx);
    region_offset = lockptr->region_offset;
    region_length = lockptr->region_length;
    if ((uchar) r.bx)   // if BL == 1, UNLOCK
    {
      // Call UNLOCK REGION function
    }
    else        // if BL == 0, LOCK
    {
      // Call LOCK REGION function
    }
  }
  else {
    // In v3.x, lock info is in regs and on the stack
    region_offset = ((ulong) r.cx << 16) + r.dx;
    region_length = ((ulong) r.si << 16) + *stack_param_ptr;

    // Call LOCK REGION function
  }
  return;
}

/* UnLock file - subfunction 0Bh */

/* We support this function only to illustrate how it works. The following
        function supports only unlocking before DOS 4.0 */
void unlockfil(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  ulong region_offset;
  ulong region_length;

  // In v3.x, lock info is in regs and on the stack
  region_offset = ((ulong) r.cx << 16) + r.dx;
  region_length = ((ulong) r.si << 16) + *stack_param_ptr;

  // Call UNLOCK REGION function

  return;
}

/* Get Disk Space - subfunction 0Ch */
void dskspc(void)
{
  r.ax = 1;
  r.bx = total_sectors;
  r.cx = SECTOR_SIZE;
  r.dx = free_sectors;
}

/* Get File Attributes - subfunction 0Fh */
void getfatt(void)
{
  if (contains_wildcards(fcbname_ptr)) {
    fail(2);
    return;
  }

  *srch_attr_ptr = 0x3f;
  ffirst();
  if (r.ax)
    return;

  r.ax = (uint) dirrec_ptr->file_attr;
}

/* Set File Attributes - subfunction 0Eh */
void setfatt()
{
  getfatt();
  if (r.flags & FCARRY)
    return;

  if ((((uchar) *stack_param_ptr) & 0x10) ||
      (((DIRREC_PTR) sector_buffer)[srchrec_ptr->dir_entry_no].file_attr & 0x10)) {
    fail(5);
    return;
  }

  ((DIRREC_PTR) sector_buffer)[srchrec_ptr->dir_entry_no].file_attr = (uchar) *stack_param_ptr;
  if (!put_sector(last_sector, sector_buffer)) {
    fail(5);
    return;
  }
}

/* Rename File - subfunction 11h */
void renfil(void)
{
  char far *path;
  uint ret = 0, dir_sector;
  int i = 0, j;

  *srch_attr_ptr = 0x21;
  srchrec_ptr_2->attr_mask = 0x3f;
  ffirst();
  if (r.ax)
    return;

  if (path = _fstrrchr(filename_ptr_2, '\\'))
    *path++ = 0;

  /* Keep the new name mask in fcbname_ptr_2 */
  _fmemset(fcbname_ptr_2, ' ', 11);
  for (; *path; path++)
    switch (*path) {
    case '.':
      i = 8;
      break;
    case '*':
      j = (i < 8) ? 8 : 11;
      while (i < j)
        fcbname_ptr_2[i++] = '?';
      break;
    default:
      fcbname_ptr_2[i++] = *path;
    }
  _fmemcpy(srchrec_ptr_2->srch_mask, fcbname_ptr_2, 11);
  if ((ret = ffirst2()) == 3) {
    fail(3);
    return;
  }
  else if (!ret) {
    fail(5);
    return;
  }

  ret = 0;
  dir_sector = srchrec_ptr_2->dir_sector;

  while (!r.ax) {
    for (i = 0; i < 11; i++)
      srchrec_ptr_2->srch_mask[i] = (fcbname_ptr_2[i] == '?')
        ? dirrec_ptr->file_name[i]
        : fcbname_ptr_2[i];
    if ((dirrec_ptr->file_attr & 1) || (!ffirst2()))
      ret = 5;
    else {
      if (!create_dir_entry(&dir_sector, NULL, srchrec_ptr_2->srch_mask,
                            dirrec_ptr->file_attr, dirrec_ptr->start_sector,
                            dirrec_ptr->file_size, dirrec_ptr->file_time))
        ret = 5;
      else {
        if (!get_sector(last_sector = srchrec_ptr->dir_sector, sector_buffer)) {
          fail(5);
          return;
        }
        ((DIRREC_PTR) sector_buffer)[srchrec_ptr->dir_entry_no].file_name[0] = (char) 0xE5;
        if (!put_sector(srchrec_ptr->dir_sector, sector_buffer)) {
          fail(5);
          return;
        }
      }
    }
    fnext();
  }

  if (r.ax == 18)
    r.ax = ret;

  if (!r.ax)
    succeed();
  else
    fail(r.ax);
}

/* Delete File - subfunction 13h */
void delfil(void)
{
  uint ret = 0;

  *srch_attr_ptr = 0x21;
  ffirst();

  while (!r.ax) {
    if (dirrec_ptr->file_attr & 1)
      ret = 5;
    else {
      FREE_SECTOR_CHAIN(dirrec_ptr->start_sector);
      ((DIRREC_PTR) sector_buffer)[srchrec_ptr->dir_entry_no].file_name[0] = (char) 0xE5;
      if (      /* dirsector_has_entries(last_sector, sector_buffer) && */
           (!put_sector(last_sector, sector_buffer))) {
        fail(5);
        return;
      }
    }
    fnext();
  }

  if (r.ax == 18)
    r.ax = ret;

  if (!r.ax)
    succeed();
  else
    fail(r.ax);
}

/* Support functions for the various file open functions below */

void init_sft(SFTREC_PTR p)
{
  /*
     Initialize the supplied SFT entry. Note the modifications to
     the open mode word in the SFT. If bit 15 is set when we receive
     it, it is an FCB open, and requires the Set FCB Owner internal
     DOS function to be called.
   */
  if (p->open_mode & 0x8000)
    /* File is being opened via FCB */
    p->open_mode |= 0x00F0;
  else
    p->open_mode &= 0x000F;

  /* Mark file as being on network drive, unwritten to */
  p->dev_info_word = (uint) (0x8040 | (uint) our_drive_no);
  p->file_pos = 0;
  p->rel_sector = 0xffff;
  p->abs_sector = 0xffff;
  p->dev_drvr_ptr = NULL;

  if (p->open_mode & 0x8000)
    set_sft_owner(p);
}

/* Note that the following function uses dirrec_ptr to supply much of
   the SFT data. This is because an open of an existing file is
   effectively a findfirst with data returned to the caller (DOS) in
   an SFT, rather than a found file directory entry buffer. So this
   function uses the knowledge that it is immediately preceded by a
   ffirst(), and that the data is avalable in dirrec_ptr. */

void fill_sft(SFTREC_PTR p, int use_found_1, int truncate)
{
  _fmemcpy(p->file_name, fcbname_ptr, 11);
  if (use_found_1) {
    p->file_attr = dirrec_ptr->file_attr;
    p->file_time = truncate ? dos_ftime() : dirrec_ptr->file_time;
    if (truncate) {
      FREE_SECTOR_CHAIN(dirrec_ptr->start_sector);
      p->start_sector = 0xFFFF;
    }
#if 0
    else
      p->start_sector = dirrec_ptr->start_sector;
#endif
    p->file_size = truncate ? 0L : dirrec_ptr->file_size;
    p->dir_sector = srchrec_ptr->dir_sector;
    p->dir_entry_no = (uchar) srchrec_ptr->dir_entry_no;
  }
  else {
    p->file_attr = (uchar) *stack_param_ptr;   /* Attr is top of stack */
    p->file_time = dos_ftime();
    p->start_sector = 0xffff;
    p->file_size = 0;
#ifndef DO_FUJI
    p->dir_sector = srchrec_ptr->dir_sector;
#endif
    p->dir_entry_no = 0xff;
  }
}

/* Open Existing File - subfunction 16h */
void opnfil(void)
{
  SFTREC_PTR p;

  /* locate any file for any open */

  p = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (contains_wildcards(fcbname_ptr)) {
    fail(3);
    return;
  }

  *srch_attr_ptr = 0x27;
  ffirst();
  if (!r.ax) {
    fill_sft(p, TRUE, FALSE);
    init_sft(p);
  }
}

/* Truncate/Create File - subfunction 17h */
void creatfil(void)
{
  SFTREC_PTR p = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (contains_wildcards(fcbname_ptr)) {
    fail(3);
    return;
  }

  *srch_attr_ptr = 0x3f;
  ffirst();
  if ((r.flags & FCARRY) && (r.ax != 2))
    return;

  if ((!r.ax) && (dirrec_ptr->file_attr & 0x19)) {
    fail(5);
    return;
  }

  fill_sft(p, (!r.ax), TRUE);
  init_sft(p);
  succeed();
}

/* This function is never called! DOS fiddles with position internally */
void skfmend(void)
{
  long seek_amnt;
  SFTREC_PTR p;

  /* But, just in case... */
  seek_amnt = -1L * (((long) r.cx << 16) + r.dx);
  p = (SFTREC_PTR) MK_FP(r.es, r.di);
  if (seek_amnt > p->file_size)
    seek_amnt = p->file_size;

  p->file_pos = p->file_size - seek_amnt;
  r.dx = (uint) (p->file_pos >> 16);
  r.ax = (uint) (p->file_pos & 0xFFFF);
}

void unknown_fxn_2D()
{
  r.ax = 2;
  /* Only called in v4.01, this is what MSCDEX returns */
}

/* Special Multi-Purpose Open File - subfunction 2Eh */

#define CREATE_IF_NOT_EXIST             0x10
#define OPEN_IF_EXISTS                  0x01
#define REPLACE_IF_EXISTS               0x02

void special_opnfil(void)
{
  SFTREC_PTR p = (SFTREC_PTR) MK_FP(r.es, r.di);
  uint open_mode, action;

  open_mode = ((V4_SDA_PTR) sda_ptr)->mode_2E & 0x7f;
  action = ((V4_SDA_PTR) sda_ptr)->action_2E;
  p->open_mode = open_mode;

  if (contains_wildcards(fcbname_ptr)) {
    fail(3);
    return;
  }

  *srch_attr_ptr = 0x3f;
  ffirst();
#if 1
  {
    int err;
    fujifs_handle handle;


    err = fujifs_open(0, &handle, "tnfs://10.4.0.1/randata.bin", FUJIFS_READ);
    if (err == NETWORK_ERROR_FILE_NOT_FOUND) {
      fail(DOSERR_FILE_NOT_FOUND);
      return;
    }
    else if (err) {
#ifdef DEBUG
      consolef("FN OPEN_EXTENDED fail %i\n", err);
#endif
      fail(DOSERR_READ_FAULT);
      return;
    }

    p->FNSFT_HANDLE = handle;
    consolef("FN OPEN_EXTENDED assigned %i\n", p->FNSFT_HANDLE);
  }
#else
  if ((r.flags & FCARRY) && (r.ax != 2))
    return;

  if (!r.ax) {
    if ((dirrec_ptr->file_attr & 0x18) ||
        ((dirrec_ptr->file_attr & 0x01) && (open_mode & 3)) || (!(action &= 0x000F))) {
      fail(5);
      return;
    }
  }
  else {
    if (!(action &= 0x00F0)) {
      fail(2);
      return;
    }
  }

  if ((!(open_mode & 3)) && r.ax) {
    fail(5);
    return;
  }
#endif

  fill_sft(p, (!r.ax), action & REPLACE_IF_EXISTS);
  init_sft(p);
  dumpHex(p, sizeof(*p), 0);
  succeed();
}

/* A placeholder */
void unsupported(void)
{
  return;
}

typedef void (*PROC)(void);

PROC dispatch_table[] = {
  inquiry,              /* 0x00h */
  rd,                   /* 0x01h */
  unsupported,          /* 0x02h */
  md,                   /* 0x03h */
  unsupported,          /* 0x04h */
  cd,                   /* 0x05h */
  clsfil,               /* 0x06h */
  cmmtfil,              /* 0x07h */
  readfil,              /* 0x08h */
  writfil,              /* 0x09h */
  lockfil,              /* 0x0Ah */
  unlockfil,            /* 0x0Bh */
  dskspc,               /* 0x0Ch */
  unsupported,          /* 0x0Dh */
  setfatt,              /* 0x0Eh */
  getfatt,              /* 0x0Fh */
  unsupported,          /* 0x10h */
  renfil,               /* 0x11h */
  unsupported,          /* 0x12h */
  delfil,               /* 0x13h */
  unsupported,          /* 0x14h */
  unsupported,          /* 0x15h */
  opnfil,               /* 0x16h */
  creatfil,             /* 0x17h */
  unsupported,          /* 0x18h */
  unsupported,          /* 0x19h */
  unsupported,          /* 0x1Ah */
  ffirst,               /* 0x1Bh */
  fnext,                /* 0x1Ch */
  unsupported,          /* 0x1Dh */
  unsupported,          /* 0x1Eh */
  unsupported,          /* 0x1Fh */
  unsupported,          /* 0x20h */
  skfmend,              /* 0x21h */
  unsupported,          /* 0x22h */
  unsupported,          /* 0x23h */
  unsupported,          /* 0x24h */
  unsupported,          /* 0x25h */
  unsupported,          /* 0x26h */
  unsupported,          /* 0x27h */
  unsupported,          /* 0x28h */
  unsupported,          /* 0x29h */
  unsupported,          /* 0x2Ah */
  unsupported,          /* 0x2Bh */
  unsupported,          /* 0x2Ch */
  unknown_fxn_2D,       /* 0x2Dh */
  special_opnfil        /* 0x2Eh */
};

#define MAX_FXN_NO (sizeof(dispatch_table) / sizeof(PROC))

/* Split the last level of the path in the filname field of the
        SDA into the FCB-style filename area, also in the SDA */

void get_fcbname_from_path(char far *path, char far *fcbname)
{
  int i;

  _fmemset(fcbname, ' ', 11);
  for (i = 0; *path; path++)
    if (*path == '.')
      i = 8;
    else
      fcbname[i++] = *path;
}

/* This function should not be necessary. DOS usually generates an FCB
   style name in the appropriate SDA area. However, in the case of
   user input such as 'CD ..' or 'DIR ..' it leaves the fcb area all
   spaces. So this function needs to be called every time. Its other
   feature is that it uses an internal DOS call to determine whether
   the filename is a DOS character device. We will 'Access deny' any
   use of a char device explicitly directed to our drive */

void generate_fcbname(uint dos_ds)
{
  get_fcbname_from_path((char far *) (_fstrrchr(filename_ptr, '\\') + 1), fcbname_ptr);

  filename_is_char_device = is_a_character_device(dos_ds);
}

int is_call_for_us(uint es, uint di, uint ds)
{
  uchar far *p;
  int ret = 0xFF;

  filename_is_char_device = 0;

  // Note that the first 'if' checks for the bottom 6 bits
  // of the device information word in the SFT. Values > last drive
  // relate to files not associated with drives, such as LAN Manager
  // named pipes (Thanks to Dave Markun).
  if ((curr_fxn >= _clsfil && curr_fxn <= _unlockfil)
      || (curr_fxn == _skfmend)
      || (curr_fxn == _unknown_fxn_2D)) {
    ret = ((((SFTREC_PTR) MK_FP(es, di))->dev_info_word & 0x3F)
           == our_drive_no);
  }
  else {
    if (curr_fxn == _inquiry)   // 2F/1100 -- succeed automatically
      ret = TRUE;
    else {
      if (curr_fxn == _fnext)   // Find Next
      {
        SRCHREC_PTR psrchrec;   // check search record in SDA

        if (_osmajor == 3)
          psrchrec = &(((V3_SDA_PTR) sda_ptr)->srchrec);
        else
          psrchrec = &(((V4_SDA_PTR) sda_ptr)->srchrec);
        return ((psrchrec->drive_no & (uchar) 0x40) &&
                ((psrchrec->drive_no & (uchar) 0x1F) == our_drive_no));
      }
      if (_osmajor == 3)
        p = ((V3_SDA_PTR) sda_ptr)->cdsptr;     // check CDS
      else
        p = ((V4_SDA_PTR) sda_ptr)->cdsptr;

      if (_fmemcmp(cds_path_root, p, cds_root_size) == 0) {
        // If a path is present, does it refer to a character device
        if (curr_fxn != _dskspc)
          generate_fcbname(ds);
        return TRUE;
      }
      else
        return FALSE;
    }
  }
  return ret;
}

/* -------------------------------------------------------------*/

/* This is the main entry point for the redirector. It assesses if
   the call is for our drive, and if so, calls the appropriate routine. On
   return, it restores the (possibly modified) register values. */

void interrupt far redirector(ALL_REGS entry_regs)
{
  static uint save_bp;
  uint our_ss, our_sp, cur_ss, cur_sp;

  _asm STI;

  if (((entry_regs.ax >> 8) != (uchar) 0x11) || ((uchar) entry_regs.ax > MAX_FXN_NO))
    goto chain_on;

  curr_fxn = fxnmap[(uchar) entry_regs.ax];

  if ((curr_fxn == _unsupported) ||
      (!is_call_for_us(entry_regs.es, entry_regs.di, entry_regs.ds)))
    goto chain_on;

  /* Set up our copy of the registers */
  r = entry_regs;

  // Save ss:sp and switch to our internal stack. We also save bp
  // so that we can get at any parameter at the top of the stack
  // (such as the file attribute passed to subfxn 17h).
  _asm mov dos_ss, ss;
  _asm mov save_bp, bp;

  stack_param_ptr = (uint far *) MK_FP(dos_ss, save_bp + sizeof(ALL_REGS));

  cur_ss = getSS();
  cur_sp = getSP();
  our_sp = (FP_OFF(our_stack) + 15) >> 4;
  our_ss = FP_SEG(our_stack) + our_sp;
  our_sp = STACK_SIZE - 2 - (((our_sp - (FP_OFF(our_stack) >> 4)) << 4)
			     - (FP_OFF(our_stack) & 0xf));
#if 0
#ifdef DEBUG
  consolef("STACK 0x%08lx SS:SP=%04x:%04x OUR=%04x:%04x FP=%04x:%04x DS=%04x\n",
	   (void far *) our_stack, cur_ss, cur_sp,
	   our_ss, our_sp, FP_SEG(our_stack), FP_OFF(our_stack), getDS());
#endif
#endif
    
  _asm {
    mov dos_sp, sp;

    mov ax, our_ss;
    mov cx, our_sp;
      
    // activate new stack
    cli;
    mov ss, ax;
    mov sp, cx;
    sti;
  }

  cur_ss = getSS();
  cur_sp = getSP();
#if 0
#ifdef DEBUG
  consolef("SS:SP=%04x:%04x DOS=%04x:%04x\n", cur_ss, cur_sp, dos_ss, dos_sp);
#endif
#endif

  // Expect success!
  succeed();

#define DEBUG_DISPATCH
#if defined(DEBUG_DISPATCH) && defined(DEBUG)
  consolef("DISPATCH IN 0x%02x\n", curr_fxn);
#endif
  // Call the appropriate handling function unless we already know we
  // need to fail
  if (filename_is_char_device)
    fail(5);
  else
    dispatch_table[curr_fxn]();
#if defined(DEBUG_DISPATCH) && defined(DEBUG)
  consolef("DISPATCH OUT err: %i result: 0x%04x\n", r.flags & FCARRY, r.ax);
#endif

  // Switch the stack back
  _asm {
    cli;
    mov ss, dos_ss;
    mov sp, dos_sp;
    sti;
  }

  cur_ss = getSS();
  cur_sp = getSP();
#if 0
#ifdef DEBUG
  consolef("restored SS:SP=%04x:%04x DOS=%04x:%04x\n", cur_ss, cur_sp, dos_ss, dos_sp);
#endif
#endif
  
  // put the possibly changed registers back on the stack, and return
  entry_regs = r;
  return;

  // If the call wasn't for us, we chain on.
 chain_on:
  _chain_intr(prev_int2f_vector);
}

const char *makeitwork = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
