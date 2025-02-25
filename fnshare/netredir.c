#include "netredir.h"
#include "dosdata.h"
#include "doserr.h"
#include "fujicom.h"
#include "../ncopy/fujifs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <ctype.h>
#include "../sys/print.h"

/* Get value of field from [S]wappable [D]OS [A]rea */
#define DOS_SDA_VALUE(x) DOS_STRUCT_VALUE(SDA_PTR, sda_ptr, x)
#define DOS_SDA_V4_VALUE(x) (((SDA_PTR_V4) sda_ptr)->x)

/* Get pointer to field */
#define DOS_SDA_POINTER(x) DOS_STRUCT_POINTER(SDA_PTR, sda_ptr, x)

void interrupt far (*old_int2f)();
void far *sda_ptr;
uint8_t drive_num;

static char temp_path[DOS_MAX_PATHLEN];

typedef uint16_t(*redirectFunction_t)(void far *parms);

char *undosify_path(const char far *path)
{
  const char far *backslash;
  int idx;


  // remove drive root
  backslash = _fstrchr(path, '\\');
  if (!backslash)
    return NULL;
  _fstrncpy(temp_path, backslash, sizeof(temp_path));

  for (idx = 0; temp_path[idx]; idx++) {
    if (temp_path[idx] == '\\')
      temp_path[idx] = '/';
    else // FIXME - FujiNet should be handling the case fixing
      temp_path[idx] = tolower(temp_path[idx]);
  }

  return temp_path;
}

int contains_wildcards(char far *path)
{
  int idx;


  for (idx = 0; idx < DOS_FILENAME_LEN; idx++)
    if (path[idx] == '?')
      return 1;
  return 0;
}

int filename_match(char far *pattern, char far *filename)
{
  int idx, jdx;


#if 0
  consolef("COMPARING \"%ls\" \"%ls\"\n", pattern, filename);
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

    if (pattern[idx] != toupper(filename[idx])) {
#if 0
      consolef("POS MISMATCH: %c != %c\n", pattern[idx], toupper(filename[idx]));
#endif
      return 0;
    }
  }

  // If disk name still has characters before '.', mismatch
  if (filename[idx] && filename[idx] != '.') {
#if 0
    consolef("FILENAME TOO LONG\n");
#endif
    return 0;
  }

  // Skip '.' in disk name if present
  if (filename[idx] == '.')
    idx++;
  jdx = idx;

  // Compare the extension
  for (idx = 8; idx < DOS_FILENAME_LEN; idx++, jdx++) {
    if (pattern[idx] == ' ')
      break; // End of extension in pattern

    if (pattern[idx] == '?') {
      if (!filename[jdx])
        break; // '?' matches even if extension is shorter
      continue;
    }

    if (pattern[idx] != toupper(filename[jdx])) {
#if 0
      consolef("EXT MISMATCH: %c != %c\n", pattern[idx], toupper(filename[jdx]));
#endif
      return 0;
    }
  }

#if 0
  if (filename[jdx]) {
    consolef("CHARS REMAINING %i %i\n", idx, jdx);
    return 0;
  }
#endif
  return !filename[jdx];
}

int findnext(SRCHREC_PTR search)
{
  FN_DIRENT *ent;
  int len;
  char far *pattern;
  const char *dot, *ext;
  uint8_t search_attr;
  DIRREC_PTR dos_entry;


  // FIXME - make these arguments instead of accessing globals
  dos_entry = DOS_SDA_POINTER(dirrec);
  pattern = DOS_SDA_VALUE(fcb_name);
  search_attr = search->attr_mask;

  search->sequence++;
  //consolef("NX %i PATTERN: %ls\n", search->sequence, pattern);

  while (1) {
    ent = fujifs_readdir();
    if (!ent)
      return DOSERR_NO_MORE_FILES;
    if (filename_match(pattern, ent->name))
      break;
  }

#if 0
  consolef("ENTRY: \"%s\"\n", ent->name);

  if (search->sequence > 2) {
    for (;;)
      ;
  }
#endif

  search->drive_num = (drive_num + 1) | 0x80;
  _fmemmove(search->pattern, pattern, sizeof(search->pattern));
  search->attr_mask = search_attr;
  search->sector = 0; // PHANTOM sets this to path length

  _fmemset(dos_entry->name, ' ', sizeof(search->pattern));
  dot = strchr(ent->name, '.');
  if (dot)
    ext = dot + 1;
  else {
    dot = ent->name + strlen(ent->name);
    ext = NULL;
  }
  len = dot - ent->name;
  _fmemcpy(dos_entry->name, ent->name, len <= 8 ? len : 8);
  if (ext) {
    len = strlen(ext);
    _fmemcpy(&dos_entry->name[8], ext, len <= 3 ? len : 3);
  }

  dos_entry->attr = ent->isdir ? ATTR_DIRECTORY : 0;

  dos_entry->time = (ent->mtime.tm_sec / 2)
    | (ent->mtime.tm_min << 5) | (ent->mtime.tm_hour << 11);
  dos_entry->date = (ent->mtime.tm_mday)
    | ((ent->mtime.tm_mon + 1) << 5) | ((ent->mtime.tm_year - 80) << 9);

  dos_entry->size = ent->size;

  return DOSERR_NONE;
}

int findfirst(const char far *path, SRCHREC_PTR search)
{
  errcode err;
  DIRREC_PTR dos_entry;
  char far *pattern;
  uint8_t search_attr;


  //consolef("PATH: %ls\n", DOS_SDA_VALUE(path1));
  // FIXME - make these arguments instead of accessing globals
  dos_entry = DOS_SDA_POINTER(dirrec);
  pattern = DOS_SDA_VALUE(fcb_name);
  search_attr = DOS_SDA_VALUE(srch_attr);

  //consolef("FF PATTERN: 0x%02x %ls\n", search_attr, pattern);

  if (search_attr & ATTR_VOLUME_LABEL) {
    //consolef("VOLUME\n");
    search->drive_num = (drive_num + 1) | 0x80;
    _fmemmove(search->pattern, pattern, sizeof(search->pattern));
    search->attr_mask = search_attr;
    search->sequence = 0;
    search->sector = 0; // PHANTOM sets this to path length
    _fstrcpy(dos_entry->name, "FUJINET1234");
    dos_entry->attr = ATTR_VOLUME_LABEL;
    dos_entry->time = 0;
    dos_entry->date = 0;
    dos_entry->size = 0;

    //dumpHex(search, sizeof(*search), 0);
    //dumpHex(dos_entry, sizeof(*dos_entry), 0);
    return DOSERR_NONE;
  }

  // FIXME - was directory already open?
  //fujifs_closedir();

  err = fujifs_opendir("");
  if (err)
    return DOSERR_UNEXPECTED_NETWORK_ERROR;

  return findnext(search);
}

int chdir()
{
  char far *new_dir;
  errcode err;


  new_dir = DOS_SDA_VALUE(path1);

#if 0
  consolef("CWD: \"%ls\"\n", DOS_SDA_VALUE(cdsptr));
  consolef("NEW: \"%ls\"\n", new_dir);
#endif
  new_dir = undosify_path(new_dir);
#if 0
  consolef("CD TO \"%ls\"\n", new_dir);
#endif

  /* Try to open new_path as a directory first, if it fails then it
     either doesn't exist or wasn't a directory */
  err = fujifs_opendir(new_dir);
  if (err)
    return DOSERR_PATH_NOT_FOUND;

  fujifs_chdir(new_dir);
  return DOSERR_NONE;
}

int open_extended(SFTREC_PTR sft)
{
  char *path;
  uint16_t mode, action, attr;
  int err;


  mode = DOS_SDA_V4_VALUE(mode_2E) & 0x7F;

  // FIXME - if not opening read-only then error out for now
  if (mode & ~MODE_DENYNONE) {
    consolef("FN OPEN_EXTENDED Unsupported mode 0x%04x\n", mode);
    return DOSERR_DISK_WRITE_PROTECTED;
  }

  path = undosify_path(DOS_SDA_VALUE(path1));
  action = DOS_SDA_V4_VALUE(action_2E);
  attr = DOS_SDA_V4_VALUE(attr_2E);
#if 0
  consolef("OPEN EXT: mode 0x%04x action 0x%04x attr 0x%04x \"%s\"\n",
           mode, action, attr, path);
#endif

  // FIXME - using findfirst() to stat the file. Requires reading the entire directory.
  {
    DIRREC_PTR dos_entry;


    //consolef("FCB/PATTERN %ls\n", DOS_SDA_VALUE(fcb_name));

    _fmemcpy(sft->file_name, DOS_SDA_VALUE(fcb_name), DOS_FILENAME_LEN);
    findfirst(DOS_SDA_VALUE(path1), DOS_SDA_POINTER(srchrec));
    dos_entry = DOS_SDA_POINTER(dirrec);

    sft->file_pos = 0;

    sft->file_attr = dos_entry->attr;
    sft->file_time = dos_entry->time;
    sft->file_date = dos_entry->date;
    sft->file_size = dos_entry->size;

    sft->dev_drvr_ptr = NULL;
    sft->start_sector = 20;
    sft->dir_sector = 0;
    sft->rel_sector = sft->abs_sector = 0xffff;
    sft->dev_drvr_ptr = 0;

    sft->open_mode = mode;
    // FIXME - use constants
    if (sft->open_mode & 0x8000)
      /* File is being opened via FCB */
      sft->open_mode |= 0x00F0;
    else
      sft->open_mode &= 0x000F;

    // FIXME - use constant
    sft->dev_info_word = 0x8040 | drive_num; // Mark file as being on network drive

    sft->dir_entry_no = 0xff;
  }

  // FIXME - how to open multiple files?
  err = fujifs_open(path, FUJIFS_READ);
  if (err == NETWORK_ERROR_FILE_NOT_FOUND) {
    consolef("FN OPEN_EXTENDED not found: %s\n", path);
    return DOSERR_FILE_NOT_FOUND;
  }
  else if (err) {
    consolef("FN OPEN_EXTENDED fail %i\n", err);
    return DOSERR_READ_FAULT;
  }

  return DOSERR_NONE;
}

int close_file(SFTREC_PTR sft)
{
  fujifs_close();
  return DOSERR_NONE;
}

int read_file(SFTREC_PTR sft, uint16_t far *len_ptr)
{
  uint16_t rlen;


  rlen = fujifs_read(DOS_SDA_VALUE(current_dta), *len_ptr);
  sft->file_pos += rlen;
  *len_ptr = rlen;

  return DOSERR_NONE;
}

void __interrupt redirector(union INTPACK regs)
{
  uint8_t func, subfunc;
  char far *path;
  uint16_t result;


  func = regs.h.ah;
  subfunc = regs.h.al;

  if (func != REDIRECTOR_FUNC) { // Not our redirector call, pass it down the chain
    goto not_us;
  }

  //consolef("FN REDIR 0x%02x/%02x\n", func, subfunc);

  if (subfunc == SUBF_INQUIRY) {
    regs.x.ax = 0x00ff;
    regs.x.flags &= ~INTR_CF;
    return;
  }

  // FIXME - check if drive is us

  // Check if path is us
  path = DOS_SDA_VALUE(cdsptr);
  if (_fstrncmp(path, "FujiNet ", 8) != 0)
    goto not_us;

  switch (subfunc) {
  case SUBF_FINDFIRST:
    result = findfirst(path, DOS_SDA_POINTER(srchrec));
    break;

  case SUBF_FINDNEXT:
    result = findnext(DOS_SDA_POINTER(srchrec));
    break;

  case SUBF_CHDIR:
    result = chdir();
    break;

  case SUBF_OPENEXIST:
  case SUBF_OPENEXTENDED:
    result = open_extended(MK_FP(regs.x.es, regs.x.di));
    break;

  case SUBF_CLOSE:
    result = close_file(MK_FP(regs.x.es, regs.x.di));
    break;

  case SUBF_READ:
    result = read_file(MK_FP(regs.x.es, regs.x.di), &regs.x.cx);
    break;

  case SUBF_GETDISKSPACE:
    // FIXME - FujiNet does not support yet
    result = DOSERR_UNKNOWN_COMMAND;
    break;

  case SUBF_QUALIFYPATH:
  case SUBF_REDIRPRINTER:
    goto not_us;

  default:
    consolef("FN REDIR unimplemented function 0x%02x\n", subfunc);
    result = DOSERR_UNKNOWN_COMMAND;
    break;
  }

  //consolef("FN REDIR 0x%02x/%02x  RESULT: 0x%04x\n", func, subfunc, result);

  regs.x.ax = result;
  if (regs.x.ax)
    regs.x.flags |= INTR_CF;
  else
    regs.x.flags &= ~INTR_CF;
  return;

 not_us:
  _chain_intr(old_int2f);
}
