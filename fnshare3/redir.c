#include "redir.h"
#include "doserr.h"
#include "dosfunc.h"
#include <fujifs.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#undef DEBUG
#undef DEBUG_DISPATCH
#ifdef DEBUG
#include "debug.h"
#endif

#define STACK_SIZE 1024

ALL_REGS r;                     /* Global save area for all caller's regs */
uint8_t fn_drive_num;                           /* A: is 1, B: is 2, etc. */
fujifs_handle fn_host;
char *fn_volume;
char fn_cwd[DOS_MAX_PATHLEN+1];
static char temp_path[DOS_MAX_PATHLEN+1];

char our_drive_str[3] = " :";   /* Our drive letter string */
char far *cds_path_root = "FujiNet  :\\";       /* Root string for CDS */
uint16_t cds_root_size;             /* Size of our CDS root string */
uint16_t far *stack_param_ptr;      /* ptr to word at top of stack on entry */
int curr_fxn;                   /* Record of function in progress */
int filename_is_char_device;    /* generate_fcbname found character device name */
INTVECT prev_int2f_vector;      /* For chaining, and restoring on unload */

uint16_t dos_ss;                    /* DOS's saved SS at entry */
uint16_t dos_sp;                    /* DOS's saved SP at entry */
uint16_t our_sp;                    /* SP to switch to on entry */
uint16_t save_sp;                   /* SP saved across internal DOS calls */
char our_stack[STACK_SIZE];     /* our internal stack */

/* these are version independent pointers to various frequently used
        locations within the various DOS structures */

DIRREC_PTR dirrec_ptr1;         /* ptr to 1st found dir entry area in SDA */
DIRREC_PTR dirrec_ptr2;         /* ptr to 1st found dir entry area in SDA */
SRCHREC_PTR srchrec_ptr1;       /* ptr to 1st Search Data Block in SDA */
SRCHREC_PTR srchrec_ptr2;       /* ptr to 2nd Search Data Block in SDA */
char far *current_path;         /* ptr to current path in CDS */
char far *fcbname_ptr1;         /* ptr to 1st FCB-style name in SDA */
char far *fcbname_ptr2;         /* ptr to 2nd FCB-style name in SDA */
char far *filename_ptr1;        /* ptr to 1st filename area in SDA */
char far *filename_ptr2;        /* ptr to 2nd filename area in SDA */
uint8_t far *sda_ptr;           /* ptr to SDA */
uint8_t far *srch_attr_ptr;     /* ptr to search attribute in SDA */
#ifdef __WATCOMC__
#define FCARRY                          INTR_CF
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

void fail(uint16_t err)
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

  for (i = 0; i < DOS_FCBNAME_LEN; i++)
    if (path[i] == '?')
      return TRUE;
  return FALSE;
}

void fcbitize(char far *dest, const char *source)
{
  const char *dot, *ext;
  int len;


  _fmemset(dest, ' ', DOS_FCBNAME_LEN);
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

#ifdef DEBUG
  consolef("FCBITIZE \"%ls\" \"%ls\"\n", (char far *) source, dest);
#endif
  return;
}

char *undosify_path(const char far *path)
{
  const char far *backslash;
  int idx;


  // remove drive root
  backslash = _fstrchr(path, '\\');
  if (!backslash) {
    temp_path[0] = 0;
    return temp_path;
  }
  _fstrncpy(temp_path, backslash, sizeof(temp_path));

  for (idx = 0; temp_path[idx]; idx++) {
    if (temp_path[idx] == '\\')
      temp_path[idx] = '/';
    else // FIXME - FujiNet should be handling the case fixing
      temp_path[idx] = tolower(temp_path[idx]);
  }

  return temp_path;
}

char *path_with_volume(char far *path)
{
  uint16_t len1, len2 = 0, len3;


  /* path might already point to temp_path, so figure out how long the
     prefix is so we can make room for it. */

  len1 = strlen(fn_volume) + 1;
  if (path[0] != '/') {
    len2 = strlen(fn_cwd);
    if (len2)
      len2++;
  }
  else
    path++;
  len3 = _fstrlen(path);
  if (len1 + len2 + len3 > sizeof(temp_path) - 1)
    len3 = sizeof(temp_path) - 1 - len1 - len2;
  if (len3)
    _fmemmove(&temp_path[len1 + len2], path, len3);
  temp_path[len1 + len2 + len3] = 0;
  memmove(temp_path, fn_volume, len1 - 1);
  temp_path[len1 - 1] = '/';
  if (len2) {
    memmove(&temp_path[len1], fn_cwd, len2 - 1);
    temp_path[len1 + len2 - 1] = '/';
  }

  return temp_path;
}

/* ----- Redirector functions ------------------*/

/* Respond that it is OK to load another redirector */
void inquiry(void)
{
  r.ax = 0x00FF;
}

void fndirent_to_dirrec(FN_DIRENT far *ent, DIRREC_PTR dirrec)
{
  fcbitize(dirrec->fcb_name, ent->name);
  dirrec->attr = ent->isdir ? ATTR_DIRECTORY : 0;
  dirrec->year = ent->mtime.tm_year - 80;
  dirrec->month = ent->mtime.tm_mon + 1;
  dirrec->day = ent->mtime.tm_mday;
  dirrec->hour = ent->mtime.tm_hour;
  dirrec->minute = ent->mtime.tm_min;
  dirrec->second = ent->mtime.tm_sec / 2;
  dirrec->size = ent->size;
  dirrec->start_sector = 0;
  
  return;
}

/* Find_Next  - subfunction 1Ch */
void find_next(void)
{
#ifdef DIRECT_DRIVE
  if (!find_next_entry(srchrec_ptr1->pattern,
                       srchrec_ptr1->attr_mask, dirrec_ptr1->fcb_name,
                       &dirrec_ptr1->attr, &dirrec_ptr1->datetime,
                       &dirrec_ptr1->start_sector, &dirrec_ptr1->size,
                       &srchrec_ptr1->dir_sector, &srchrec_ptr1->index)) {
    fail(DOSERR_NO_MORE_FILES);
    return;
  }
#else
  char far *path;
  char *undos;
  errcode err;
  FN_DIRENT *ent;
  uint8_t handle, srch_mask, attr;


  if (srchrec_ptr1->dir_handle == -1) {
#ifdef DEBUG
    consolef("OPENING DIRECTORY \"%ls\"\n", filename_ptr1);
#endif
    if ((path = _fstrrchr(filename_ptr1, '\\')))
      *path = 0;
    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    err = fujifs_opendir(fn_host, &handle, undos);
    if (path)
      *path = '\\';
    if (err) {
      fail(DOSERR_UNEXPECTED_NETWORK_ERROR);
      return;
    }
    srchrec_ptr1->dir_handle = handle;
  }

  srch_mask = srchrec_ptr1->attr_mask;
  while (1) {
    ent = fujifs_readdir(srchrec_ptr1->dir_handle);
    if (!ent) {
      fujifs_closedir(srchrec_ptr1->dir_handle);
      srchrec_ptr1->dir_handle = -1;
      fail(DOSERR_NO_MORE_FILES);
      return;
    }

    attr = ent->isdir ? ATTR_DIRECTORY : 0;
    fcbitize(dirrec_ptr1->fcb_name, ent->name);

    if (match_to_mask(srchrec_ptr1->pattern, dirrec_ptr1->fcb_name) &&
        (!(
           ((srch_mask == ATTR_VOLUME_LABEL) && (!(attr & ATTR_VOLUME_LABEL)))
           || ((attr & ATTR_DIRECTORY) && (!(srch_mask & ATTR_DIRECTORY)))
           || ((attr & ATTR_VOLUME_LABEL) && (!(srch_mask & ATTR_VOLUME_LABEL)))
           || ((attr & ATTR_SYSTEM) && (!(srch_mask & ATTR_SYSTEM)))
           || ((attr & ATTR_HIDDEN) && (!(srch_mask & ATTR_HIDDEN)))))) {
      fndirent_to_dirrec(ent, dirrec_ptr1);
      break;
    }
  }
#endif
}

#ifdef DIRECT_DRIVE
/* Internal find_next for delete and rename processing */
uint16_t fnext2(void)
{
  return (find_next_entry(srchrec_ptr2->pattern, 0x20,
                          dirrec_ptr2->fcb_name, &dirrec_ptr2->attr,
                          NULL, NULL, NULL, &srchrec_ptr2->dir_sector,
                          &srchrec_ptr2->index)) ? 0 : 18;
}
#endif

/* Find_First - subfunction 1Bh */

/* This function looks a little odd because of the embedded call to
   find_next(). This arises from the my view that find_first is simply
   a find_next with some initialization overhead: find_first has to
   locate the directory in which find_next is to iterate, and
   initialize the SDB state to 'point to' the first entry. It then
   gets that first entry, using find_next.
   The r.ax test at the end of the function is because, to mimic
   DOS behavior, a find_first that finds no matching entry should
   return an error 2 (file not found), whereas a subsequent find_next
   that finds no matching entry should return error 18 (no more
   files). */

void find_first(void)
{
  char far *path;
  int success;

  /* Special case for volume-label-only search - must be in root */
#ifdef DIRECT_DRIVE
  if (path = (*srch_attr_ptr == ATTR_VOLUME_LABEL)
      ? filename_ptr1 : _fstrrchr(filename_ptr1, '\\'))
    *path = 0;
  success = get_dir_start_sector(filename_ptr1, &srchrec_ptr1->dir_sector);
  if (path)
    *path = '\\';
  if (!success) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }
#else
  // FIXME - make sure directory in filename_ptr1 exists
  if (*srch_attr_ptr == ATTR_VOLUME_LABEL) {
    srchrec_ptr1->drive_num = (fn_drive_num + 1) | 0x80;
    _fmemmove(srchrec_ptr1->pattern, fcbname_ptr1, sizeof(srchrec_ptr1->pattern));
    srchrec_ptr1->attr_mask = *srch_attr_ptr;
    _fstrcpy(dirrec_ptr1->fcb_name, "FUJINET    ");
    dirrec_ptr1->attr = ATTR_VOLUME_LABEL;
    dirrec_ptr1->datetime = 0L;
    dirrec_ptr1->size = 0L;
    succeed();
    return;
  }
#endif

  _fmemcpy(&srchrec_ptr1->pattern, fcbname_ptr1, DOS_FCBNAME_LEN);

  if (srchrec_ptr1->dir_handle != -1) {
    fujifs_closedir(srchrec_ptr1->dir_handle);
    srchrec_ptr1->dir_handle = -1;
  }
  srchrec_ptr1->index = -1;
  srchrec_ptr1->attr_mask = *srch_attr_ptr;
  srchrec_ptr1->drive_num = (uint8_t) (fn_drive_num | 0xC0);

  find_next();
  /* No need to check r.flags & FCARRY; if ax is 18,
     FCARRY must have been set. */
  if (r.ax == DOSERR_NO_MORE_FILES)
    r.ax = DOSERR_FILE_NOT_FOUND;   // make find_next error code suitable to find_first
}

#ifdef DIRECT_DRIVE
/* Internal find_first for delete and rename processing */
uint16_t ffirst2(void)
{
  if (!get_dir_start_sector(filename_ptr2, &srchrec_ptr2->dir_sector))
    return DOSERR_PATH_NOT_FOUND;

  srchrec_ptr2->index = -1;
  srchrec_ptr2->drive_num = (uint8_t) (fn_drive_num | 0x80);

  return fnext2();
}
#endif

/* ReMove Directory - subfunction 01h */
void remove_dir(void)
{
  /* special case for root */
  if ((*filename_ptr1 == '\\') && (!*(filename_ptr1 + 1))) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
  if (contains_wildcards(fcbname_ptr1)) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }
  _fstrcpy(filename_ptr2, filename_ptr1);
  *srch_attr_ptr = 0x10;

#ifdef DIRECT_DRIVE
  find_first();
  if (r.ax || (!(dirrec_ptr1->attr & 0x10))) {
    r.ax = DOSERR_PATH_NOT_FOUND;
    return;
  }

  if (!_fstrncmp(filename_ptr2, current_path, _fstrlen(filename_ptr2))) {
    fail(DOSERR_CANNOT_REMOVE_CURRENT_DIR);
    return;
  }

  _fmemset(srchrec_ptr2->pattern, '?', DOS_FCBNAME_LEN);
  srchrec_ptr2->attr_mask = 0x3f;

  if ((r.ax = ffirst2()) == DOSERR_PATH_NOT_FOUND) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }

  if (!r.ax) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
#else
  {
    FN_DIRENT entry;
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_stat(fn_host, undos, &entry)) {
      fail(DOSERR_PATH_NOT_FOUND);
      return;
    }

    fndirent_to_dirrec(&entry, dirrec_ptr1);
  }

  if (!(dirrec_ptr2->attr & ATTR_DIRECTORY)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

  // FIXME - make sure this isn't current directory
#endif

#ifdef DIRECT_DRIVE
  if (!get_sector(last_sector = srchrec_ptr1->dir_sector, sector_buffer)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
  ((DIRREC_PTR) sector_buffer)[srchrec_ptr1->index].fcb_name[0] = (char) 0xE5;
  if (!put_sector(last_sector, sector_buffer)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

  FREE_SECTOR_CHAIN(dirrec_ptr1->start_sector);
#else
  {
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_rmdir(fn_host, undos)) {
      fail(DOSERR_ACCESS_DENIED);
      return;
    }
  }
#endif
  succeed();
}

/* Make Directory - subfunction 03h */
void make_dir(void)
{
  /* special case for root */
  if ((*filename_ptr1 == '\\') && (!*(filename_ptr1 + 1))) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
  // can't create dir name with * or ? in it
  if (contains_wildcards(fcbname_ptr1)) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }

#ifdef DIRECT_DRIVE
  *srch_attr_ptr = 0x3f;
  find_first();
  if (r.ax == DOSERR_NONE) {        // we need error 2 here
    fail(DOSERR_FILE_EXISTS);
    return;
  }
  if (r.ax != DOSERR_FILE_NOT_FOUND)
    return;
#else
  {
    FN_DIRENT entry;
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_stat(fn_host, undos, &entry)) {
      fail(DOSERR_FILE_EXISTS);
      return;
    }

    fndirent_to_dirrec(&entry, dirrec_ptr1);
  }
#endif

#ifdef DIRECT_DRIVE
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
  if (!(dirrec_ptr1->start_sector = next_free_sector())) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
  set_next_sector(dirrec_ptr1->start_sector, 0xFFFF);
  last_sector = dirrec_ptr1->start_sector;
  if ((!put_sector(dirrec_ptr1->start_sector, sector_buffer)) ||
      (!create_dir_entry(&srchrec_ptr1->dir_sector, NULL, fcbname_ptr1, 0x10,
                         dirrec_ptr1->start_sector, 0, dos_ftime()))) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
  succeed();
#else
  {
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_mkdir(fn_host, undos)) {
      fail(DOSERR_ACCESS_DENIED);
      return;
    }
    succeed();
  }
#endif
}

/* Change Directory - subfunction 05h */
void chdir(void)
{
  /* Special case for root */
  if ((*filename_ptr1 != '\\') || (*(filename_ptr1 + 1))) {
    if (contains_wildcards(fcbname_ptr1)) {
      fail(DOSERR_PATH_NOT_FOUND);
      return;
    }

#ifdef DIRECT_DRIVE
    *srch_attr_ptr = 0x10;
    find_first();
    if (r.ax || (!(dirrec_ptr1->attr & ATTR_DIRECTORY))) {
      fail(DOSERR_PATH_NOT_FOUND);
      return;
    }
#else
    {
      FN_DIRENT entry;
      char *undos;
      errcode err;


      undos = undosify_path(filename_ptr1);
      undos = path_with_volume(undos);
      if (fujifs_stat(fn_host, undos, &entry) || !entry.isdir) {
	fail(DOSERR_ACCESS_DENIED);
	return;
      }
    }
#endif
  }
#ifdef DEBUG
  consolef("CHDIR \"%ls\"\n", filename_ptr1);
#endif
  _fstrcpy(current_path, filename_ptr1);
}

/* Close File - subfunction 06h */
void close_file(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (sft->handle_count)  /* If handle count not 0, decrement it */
    --sft->handle_count;

  /* If writing, create/update dir entry for file */
  if (!(sft->open_mode & 3))
    return;

#ifdef DIRECT_DRIVE
  if (sft->index == 0xff) {
    if (!create_dir_entry(&sft->dir_sector, &sft->index, sft->fcb_name,
                          sft->attr, sft->start_sector, sft->size, sft->datetime))
      fail(DOSERR_ACCESS_DENIED);
  }
  else {
    if ((last_sector != sft->dir_sector) && (!get_sector(sft->dir_sector, sector_buffer)))
      fail(DOSERR_ACCESS_DENIED);
    last_sector = sft->dir_sector;
    ((DIRREC_PTR) sector_buffer)[sft->index].attr = sft->attr;
    ((DIRREC_PTR) sector_buffer)[sft->index].start_sector = sft->start_sector;
    ((DIRREC_PTR) sector_buffer)[sft->index].size = sft->size;
    ((DIRREC_PTR) sector_buffer)[sft->index].datetime = sft->datetime;
    if (!put_sector(sft->dir_sector, sector_buffer))
      fail(DOSERR_ACCESS_DENIED);
  }
#else
  if (fujifs_close(sft->file_handle))
    fail(DOSERR_ACCESS_DENIED);
#endif
}

/* Commit File - subfunction 07h */
void commit_file(void)
{
  /* We support this but don't do anything... */
  return;
}

/* Read from File - subfunction 08h */
// For version that handles critical errors,
// see Undocumented DOS, 2nd edition, chapter 8
void read_file(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (sft->open_mode & 1) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

  if ((sft->pos + r.cx) > sft->size)
    r.cx = (uint16_t) (sft->size - sft->pos);

  if (!r.cx)
    return;

  /* Fill caller's buffer and update the SFT for the file */
#ifdef DIRECT_DRIVE
  read_data(&sft->pos, &r.cx,
            sft->start_sector, &sft->rel_sector, &sft->abs_sector);
#else
  if (sft->pos != sft->last_pos)
    fujifs_seek(sft->file_handle, sft->pos); // FIXME - check error
  r.cx = fujifs_read(sft->file_handle, ((SDA_PTR_V3) sda_ptr)->current_dta, r.cx);
  sft->pos += r.cx;
  sft->last_pos = sft->pos;
#endif
}

/* Write to File - subfunction 09h */
void write_file(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);

  if (!(sft->open_mode & 3)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

#ifdef DIRECT_DRIVE
  sft->datetime = dos_ftime();
#endif

  /* Take account of DOS' 0-byte-write-truncates-file rcounte */
  if (!r.cx) {
    sft->size = sft->pos;
#ifdef DIRECT_DRIVE
    chop_file(sft->pos, &sft->start_sector, &sft->rel_sector, &sft->abs_sector);
#endif
    return;
  }

  /* Write from the caller's buffer and update the SFT for the file */
#ifdef DIRECT_DRIVE
  write_data(&sft->pos, &r.cx, ((SDA_PTR_V3) sda_ptr)->current_dta,
             &sft->start_sector, &sft->rel_sector, &sft->abs_sector);
#else
  if (sft->pos != sft->last_pos)
    fujifs_seek(sft->file_handle, sft->pos); // FIXME - check error
  r.cx = fujifs_write(sft->file_handle, ((SDA_PTR_V3) sda_ptr)->current_dta, r.cx);
  sft->pos += r.cx;
  sft->last_pos = sft->pos;
#endif
  if (sft->pos > sft->size)
    sft->size = sft->pos;
}

/* Lock file - subfunction 0Ah */

/* We support this function only to illustrate how it works. We do
        not actually honor LOCK/UNLOCK requests. The following function
        supports locking only before, and both locking/unlocking after
        DOS 4.0 */
void lock_file(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  LOCKREC_PTR lockptr;
  uint32_t region_offset;
  uint32_t region_length;

  if (_osmajor > 3) {
    // In v4.0 and above, lock info is at DS:BX in a LOCKREC structure
    lockptr = (LOCKREC_PTR) MK_FP(r.ds, r.dx);
    region_offset = lockptr->region_offset;
    region_length = lockptr->region_length;
    if ((uint8_t) r.bx)   // if BL == 1, UNLOCK
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
    region_offset = ((uint32_t) r.cx << 16) + r.dx;
    region_length = ((uint32_t) r.si << 16) + *stack_param_ptr;

    // Call LOCK REGION function
  }
  return;
}

/* UnLock file - subfunction 0Bh */

/* We support this function only to illustrate how it works. The following
        function supports only unlocking before DOS 4.0 */
void unlock_file(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  uint32_t region_offset;
  uint32_t region_length;

  // In v3.x, lock info is in regs and on the stack
  region_offset = ((uint32_t) r.cx << 16) + r.dx;
  region_length = ((uint32_t) r.si << 16) + *stack_param_ptr;

  // Call UNLOCK REGION function

  return;
}

/* Get Disk Space - subfunction 0Ch */
void disk_space(void)
{
  r.ax = 1;
#ifdef DIRECT_DRIVE
  r.bx = total_sectors;
  r.dx = free_sectors;
  r.cx = SECTOR_SIZE;
#else
#warning disk_space() not implemented
  r.bx = 0;
  r.dx = 0;
  r.cx = 512;
#endif
}

/* Get File Attributes - subfunction 0Fh */
void get_attr(void)
{
  if (contains_wildcards(fcbname_ptr1)) {
    fail(DOSERR_FILE_NOT_FOUND);
    return;
  }

#ifdef DIRECT_DRIVE
  *srch_attr_ptr = 0x3f;
  find_first();
  if (r.ax)
    return;
#else
  {
    FN_DIRENT entry;
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_stat(fn_host, undos, &entry)) {
      fail(DOSERR_FILE_NOT_FOUND);
      return;
    }
    fndirent_to_dirrec(&entry, dirrec_ptr1);
  }
#endif

  r.ax = (uint16_t) dirrec_ptr1->attr;
}

/* Set File Attributes - subfunction 0Eh */
void set_attr()
{
  get_attr();
  if (r.flags & FCARRY)
    return;

#ifdef DIRECT_DRIVE
  if ((((uint8_t) *stack_param_ptr) & 0x10) ||
      (((DIRREC_PTR) sector_buffer)[srchrec_ptr1->index].attr & 0x10)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

  ((DIRREC_PTR) sector_buffer)[srchrec_ptr1->index].attr = (uint8_t) *stack_param_ptr;
  if (!put_sector(last_sector, sector_buffer)) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
#else
#warning set_attr() not implemented
#ifdef DEBUG
  consolef("SET_ATTR \"%ls\"\n", filename_ptr1);
#endif
  fail(DOSERR_ACCESS_DENIED);
  return;
#endif
}

void fcb_to_path(char far *path, char far *fcbname)
{
  char far *sep, far *space;


  if ((sep = _fstrrchr(path, '\\')))
    sep++;
  else
    sep = path;

  _fmemmove(sep, fcbname, 8);
  sep[8] = 0;

  if (fcbname[0] != ' ') {
    if (!(space = _fstrchr(sep, ' ')))
      space = sep + 8;
    *space++ = '.';
    _fmemmove(space, &fcbname[8], 3);
    space[3] = 0;
    if ((space = _fstrchr(space, ' ')))
      *space = 0;
  }

  return;
}

/* Rename File - subfunction 11h */
void rename_files(void)
{
  char far *path;
  int i = 0, j;
  uint16_t ret = DOSERR_NONE;
#ifdef DIRECT_DRIVE
  uint16_t dir_sector;
#endif

  *srch_attr_ptr = 0x21;
  srchrec_ptr2->attr_mask = 0x3f;
  find_first();
  if (r.ax)
    return;

  if (path = _fstrrchr(filename_ptr2, '\\'))
    *path++ = 0;

  /* Keep the new name pattern in fcbname_ptr2 */
  _fmemset(fcbname_ptr2, ' ', DOS_FCBNAME_LEN);
  for (; *path; path++)
    switch (*path) {
    case '.':
      i = 8;
      break;
    case '*':
      j = (i < 8) ? 8 : DOS_FCBNAME_LEN;
      while (i < j)
        fcbname_ptr2[i++] = '?';
      break;
    default:
      fcbname_ptr2[i++] = *path;
    }
  _fmemcpy(srchrec_ptr2->pattern, fcbname_ptr2, DOS_FCBNAME_LEN);
#ifdef DIRECT_DRIVE
  if ((ret = ffirst2()) == DOSERR_PATH_NOT_FOUND) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }
  else if (!ret) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }
#else
  // FIXME - make sure filename_ptr2 points to valid directory? Not
  //         sure what ffirst2() is doing above
#endif

  ret = DOSERR_NONE;
#ifdef DIRECT_DRIVE
  dir_sector = srchrec_ptr2->dir_sector;
#endif

  /* DOS makes our function handle all the wildcards instead of doing
     it itself. Wildcards are allowed on both on the source and the
     destination. We have to loop through all the entries in a
     directory. */
  while (!r.ax) {
    for (i = 0; i < DOS_FCBNAME_LEN; i++)
      srchrec_ptr2->pattern[i] = (fcbname_ptr2[i] == '?')
        ? dirrec_ptr1->fcb_name[i]
        : fcbname_ptr2[i];
    if (dirrec_ptr1->attr & ATTR_READ_ONLY)
      ret = DOSERR_ACCESS_DENIED;
#ifdef DIRECT_DRIVE
    else if (!ffirst2())
      ret = DOSERR_ACCESS_DENIED;
#else
    // FIXME - make sure filename_ptr2 points to valid directory? Not
    //         sure what ffirst2() is doing above
#endif
    else {
#ifdef DIRECT_DRIVE
      if (!create_dir_entry(&dir_sector, NULL, srchrec_ptr2->pattern,
                            dirrec_ptr1->attr, dirrec_ptr1->start_sector,
                            dirrec_ptr1->size, dirrec_ptr1->datetime))
        ret = DOSERR_ACCESS_DENIED;
      else {
        if (!get_sector(last_sector = srchrec_ptr1->dir_sector, sector_buffer)) {
          fail(DOSERR_ACCESS_DENIED);
          return;
        }
        ((DIRREC_PTR) sector_buffer)[srchrec_ptr1->index].fcb_name[0] = (char) 0xE5;
        if (!put_sector(srchrec_ptr1->dir_sector, sector_buffer)) {
          fail(DOSERR_ACCESS_DENIED);
          return;
        }
      }
#else
      {
	char *undos;


	_fstrcpy(filename_ptr2, filename_ptr1);
	fcb_to_path(filename_ptr1, dirrec_ptr1->fcb_name);
	fcb_to_path(filename_ptr2, srchrec_ptr2->pattern);

	undos = undosify_path(filename_ptr1);
	undos = path_with_volume(undos);

	// Need to put the undosified path somewhere to undosify the other path
	_fstrcpy(filename_ptr1, undos);

	undos = undosify_path(filename_ptr2);
	undos = path_with_volume(undos);
	
	if (fujifs_rename(fn_host, filename_ptr1, undos)) {
	  fail(DOSERR_ACCESS_DENIED);
	  return;
	}
      }
#endif
    }
    find_next();
  }

  if (r.ax == DOSERR_NO_MORE_FILES)
    r.ax = ret;

  if (!r.ax)
    succeed();
  else
    fail(r.ax);
}

/* Delete File - subfunction 13h */
void delete_files(void)
{
  uint16_t ret = DOSERR_NONE;

  *srch_attr_ptr = 0x21;
  find_first();

  while (!r.ax) {
    if (dirrec_ptr1->attr & 1)
      ret = DOSERR_ACCESS_DENIED;
    else {
#ifdef DIRECT_DRIVE
      FREE_SECTOR_CHAIN(dirrec_ptr1->start_sector);
      ((DIRREC_PTR) sector_buffer)[srchrec_ptr1->index].fcb_name[0] = (char) 0xE5;
      if (!put_sector(last_sector, sector_buffer)) {
        fail(DOSERR_ACCESS_DENIED);
        return;
      }
#else
      {
	char *undos;


	undos = undosify_path(filename_ptr1);
	undos = path_with_volume(undos);
	if (fujifs_unlink(fn_host, undos)) {
	  fail(DOSERR_ACCESS_DENIED);
	  return;
	}
      }
#endif
    }
    find_next();
  }

  if (r.ax == DOSERR_NO_MORE_FILES)
    r.ax = ret;

  if (!r.ax)
    succeed();
  else
    fail(r.ax);
}

/* Support functions for the various file open functions below */

void init_sft(SFTREC_PTR sft)
{
  /*
     Initialize the supplied SFT entry. Note the modifications to
     the open mode word in the SFT. If bit 15 is set when we receive
     it, it is an FCB open, and requires the Set FCB Owner internal
     DOS function to be called.
   */
  if (sft->open_mode & 0x8000)
    /* File is being opened via FCB */
    sft->open_mode |= 0x00F0;
  else
    sft->open_mode &= 0x000F;

  /* Mark file as being on network drive, unwritten to */
  sft->dev_info_word = (uint16_t) (0x8040 | (uint16_t) fn_drive_num);
  sft->pos = 0;
#ifdef DIRECT_DRIVE
  sft->rel_sector = 0xffff;
  sft->abs_sector = 0xffff;
#else
  sft->last_pos = sft->pos;
#endif
  sft->dev_drvr_ptr = NULL;

#ifdef DIRECT_DRIVE
  if (sft->open_mode & 0x8000)
    set_sft_owner(sft);
#endif
}

/* Note that the following function uses dirrec_ptr to supply much of
   the SFT data. This is because an open of an existing file is
   effectively a find_first with data returned to the caller (DOS) in
   an SFT, rather than a found file directory entry buffer. So this
   function uses the knowledge that it is immediately preceded by a
   find_first(), and that the data is avalable in dirrec_ptr. */

void fill_sft(SFTREC_PTR sft, int use_found_1, int truncate)
{
  _fmemcpy(sft->fcb_name, fcbname_ptr1, DOS_FCBNAME_LEN);
  if (use_found_1) {
    sft->attr = dirrec_ptr1->attr;
#ifdef DIRECT_DRIVE
    sft->datetime = truncate ? dos_ftime() : dirrec_ptr1->datetime;
#endif
    if (truncate) {
#ifdef DIRECT_DRIVE
      FREE_SECTOR_CHAIN(dirrec_ptr1->start_sector);
      sft->start_sector = 0xFFFF;
#endif
    }
#ifdef DIRECT_DRIVE
    else
      sft->start_sector = dirrec_ptr1->start_sector;
#endif
    sft->index = (uint8_t) srchrec_ptr1->index;
    sft->size = truncate ? 0L : dirrec_ptr1->size;
#ifdef DIRECT_DRIVE
    sft->dir_sector = srchrec_ptr1->dir_sector;
#endif
  }
  else {
    sft->attr = (uint8_t) *stack_param_ptr;   /* Attr is top of stack */
#ifdef DIRECT_DRIVE
    sft->datetime = dos_ftime();
#endif
    sft->size = 0;
    sft->index = 0xff;
#ifdef DIRECT_DRIVE
    sft->start_sector = 0xffff;
    sft->dir_sector = srchrec_ptr1->dir_sector;
#endif
  }
}

/* This function is never called! DOS fiddles with position internally */
void seek_file(void)
{
  long seek_amnt;
  SFTREC_PTR sft;

  /* But, just in case... */
  seek_amnt = -1L * (((long) r.cx << 16) + r.dx);
  sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  if (seek_amnt > sft->size)
    seek_amnt = sft->size;

  sft->pos = sft->size - seek_amnt;
  r.dx = (uint16_t) (sft->pos >> 16);
  r.ax = (uint16_t) (sft->pos & 0xFFFF);
}

void extended_attr()
{
  r.ax = 2;
  /* Only called in v4.01, this is what MSCDEX returns */
}

/* Special Multi-Purpose Open File - subfunction 2Eh */

#define CREATE_IF_NOT_EXIST             0x10
#define OPEN_IF_EXISTS                  0x01
#define REPLACE_IF_EXISTS               0x02

void open_extended(void)
{
  SFTREC_PTR sft = (SFTREC_PTR) MK_FP(r.es, r.di);
  uint16_t open_mode, action;

#ifndef DIRECT_DRIVE
#ifdef DEBUG
  consolef("SUBF=%02x  STACK=%04x  SFT=%04x  E2E=%04x  ACTION=%04x\n",
	   curr_fxn & 0xFF, *stack_param_ptr, sft->open_mode, ((SDA_PTR_V4) sda_ptr)->mode_2E,
	   ((SDA_PTR_V4) sda_ptr)->action_2E);
#endif
  if ((curr_fxn & 0xFF) == SUBF_OPENEXIST) {
    open_mode = *stack_param_ptr;
    action = OPEN_IF_EXISTS;
  }
  else if ((curr_fxn & 0xFF) == SUBF_OPENCREATE) {
    action = (*stack_param_ptr) >> 8;
    if (action)
      action = REPLACE_IF_EXISTS;
    else
      action = CREATE_IF_NOT_EXIST;
    open_mode = O_WRONLY;
    // FIXME - what about O_RDWR
  }
  else {
    open_mode = ((SDA_PTR_V4) sda_ptr)->mode_2E & 0x7f;
    sft->open_mode = open_mode;
    action = ((SDA_PTR_V4) sda_ptr)->action_2E;
  }
#else
  open_mode = ((SDA_PTR_V4) sda_ptr)->mode_2E & 0x7f;
  sft->open_mode = open_mode;
#endif

  if (contains_wildcards(fcbname_ptr1)) {
    fail(DOSERR_PATH_NOT_FOUND);
    return;
  }

#ifdef DEBUG
  consolef("OPEN MODE %04x  ACTION %04x\n", open_mode, action);
#endif
#ifdef DIRECT_DRIVE
  *srch_attr_ptr = 0x3f;
  find_first();
  if ((r.flags & FCARRY) && (r.ax != DOSERR_FILE_NOT_FOUND))
    return;
#else
  {
    FN_DIRENT entry;
    char *undos;


    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    r.ax = fujifs_stat(fn_host, undos, &entry);
    if (!r.ax)
      fndirent_to_dirrec(&entry, dirrec_ptr1);
  }
#endif

  if (!r.ax) {
    if ((dirrec_ptr1->attr & (ATTR_DIRECTORY | ATTR_VOLUME_LABEL)) ||
        ((dirrec_ptr1->attr & ATTR_READ_ONLY) &&
	 (open_mode & (MODE_WRITEONLY | MODE_READWRITE)))
	|| (!(action &= 0x000F))) {
      fail(DOSERR_ACCESS_DENIED);
      return;
    }
  }
  else {
    if (!(action &= 0x00F0)) {
      fail(DOSERR_FILE_NOT_FOUND);
      return;
    }
  }

  if ((!(open_mode & (MODE_WRITEONLY | MODE_READWRITE))) && r.ax) {
    fail(DOSERR_ACCESS_DENIED);
    return;
  }

#ifndef DIRECT_DRIVE
  {
    int fd;
    int flags;
    uint8_t handle;
    char *undos;


    // Bottom 2 bits specify read/write/rw
    flags = open_mode & 0x03;
    if (flags == MODE_READONLY)
      flags = FUJIFS_READ;
    else if (flags == MODE_WRITEONLY)
      flags = FUJIFS_WRITE;
    else if (flags == MODE_READWRITE)
      flags = FUJIFS_READWRITE;
    else {
      fail(DOSERR_INVALID_ACCESS_CODE);
      return;
    }

#if 0
    // FIXME - will need to use fujifs_stat()
    if ((action & 0xFF) & CREATE_IF_NOT_EXIST) {
      flags |= O_CREAT;
      if ((action & 0xFF) & OPEN_IF_EXISTS)
	flags |= O_EXCL;
    }
    if ((action & 0xFF) & REPLACE_IF_EXISTS)
      flags |= O_TRUNC;
#endif

#ifdef DEBUG
    consolef("FUJIFS_OPEN FLAGS 0x%04x\n", flags);
#endif
    undos = undosify_path(filename_ptr1);
    undos = path_with_volume(undos);
    if (fujifs_open(fn_host, &handle, undos, flags)) {
      fail(DOSERR_UNEXPECTED_NETWORK_ERROR);
      return;
    }
    sft->file_handle = handle;
  }
#endif

  fill_sft(sft, (!r.ax), action & REPLACE_IF_EXISTS);
  init_sft(sft);
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
  remove_dir,           /* 0x01h */
  unsupported,          /* 0x02h */
  make_dir,             /* 0x03h */
  unsupported,          /* 0x04h */
  chdir,                /* 0x05h */
  close_file,           /* 0x06h */
  commit_file,          /* 0x07h */
  read_file,            /* 0x08h */
  write_file,           /* 0x09h */
  lock_file,            /* 0x0Ah */
  unlock_file,          /* 0x0Bh */
  disk_space,           /* 0x0Ch */
  unsupported,          /* 0x0Dh */
  set_attr,             /* 0x0Eh */
  get_attr,             /* 0x0Fh */
  unsupported,          /* 0x10h */
  rename_files,          /* 0x11h */
  unsupported,          /* 0x12h */
  delete_files,          /* 0x13h */
  unsupported,          /* 0x14h */
  unsupported,          /* 0x15h */
#ifdef DIRECT_DRIVE
  open_existing,        /* 0x16h */
  open_new,             /* 0x17h */
#else
  open_extended,        /* 0x16h */
  open_extended,             /* 0x17h */
#endif
  unsupported,          /* 0x18h */
  unsupported,          /* 0x19h */
  unsupported,          /* 0x1Ah */
  find_first,           /* 0x1Bh */
  find_next,            /* 0x1Ch */
  unsupported,          /* 0x1Dh */
  unsupported,          /* 0x1Eh */
  unsupported,          /* 0x1Fh */
  unsupported,          /* 0x20h */
  seek_file,            /* 0x21h */
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
  extended_attr,        /* 0x2Dh */
  open_extended         /* 0x2Eh */
};

#define MAX_FXN_NO (sizeof(dispatch_table) / sizeof(PROC))

/* Split the last level of the path in the filname field of the
        SDA into the FCB-style filename area, also in the SDA */

void get_fcbname_from_path(char far *path, char far *fcbname)
{
  int i;

  _fmemset(fcbname, ' ', DOS_FCBNAME_LEN);
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

void generate_fcbname(uint16_t dos_ds)
{
  get_fcbname_from_path((char far *) (_fstrrchr(filename_ptr1, '\\') + 1), fcbname_ptr1);

  filename_is_char_device = is_a_character_device(dos_ds);
}

int is_call_for_us(uint16_t es, uint16_t di, uint16_t ds)
{
  uint8_t far *p;
  int ret = 0xFF;

  filename_is_char_device = 0;

  // Note that the first 'if' checks for the bottom 6 bits
  // of the device information word in the SFT. Values > last drive
  // relate to files not associated with drives, such as LAN Manager
  // named pipes (Thanks to Dave Markun).
  if ((curr_fxn >= SUBF_CLOSE && curr_fxn <= SUBF_UNLOCK)
      || (curr_fxn == SUBF_SEEK)
      || (curr_fxn == SUBF_EXTENDATTR)) {
    ret = ((((SFTREC_PTR) MK_FP(es, di))->dev_info_word & 0x3F)
           == fn_drive_num);
  }
  else {
    if (curr_fxn == SUBF_INQUIRY)   // 2F/1100 -- succeed automatically
      ret = TRUE;
    else {
      if (curr_fxn == SUBF_FINDNEXT)   // Find Next
      {
        SRCHREC_PTR psrchrec;   // check search record in SDA

        if (_osmajor == 3)
          psrchrec = &(((SDA_PTR_V3) sda_ptr)->srchrec);
        else
          psrchrec = &(((SDA_PTR_V4) sda_ptr)->srchrec);
        return ((psrchrec->drive_num & (uint8_t) 0x40) &&
                ((psrchrec->drive_num & (uint8_t) 0x1F) == fn_drive_num));
      }
      if (_osmajor == 3)
        p = ((SDA_PTR_V3) sda_ptr)->cdsptr;     // check CDS
      else
        p = ((SDA_PTR_V4) sda_ptr)->cdsptr;

      if (_fmemcmp(cds_path_root, p, cds_root_size) == 0) {
        // If a path is present, does it refer to a character device
        if (curr_fxn != SUBF_GETDISKSPACE)
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
  static uint16_t save_bp;
  uint16_t our_ss, our_sp, cur_ss, cur_sp;

  _asm STI;

  if (((entry_regs.ax >> 8) != (uint8_t) 0x11) || ((uint8_t) entry_regs.ax > MAX_FXN_NO))
    goto chain_on;

  curr_fxn = (uint8_t) entry_regs.ax;

  if ((dispatch_table[curr_fxn] == unsupported) ||
      (!is_call_for_us(entry_regs.es, entry_regs.di, entry_regs.ds)))
    goto chain_on;

  /* Set up our copy of the registers */
  r = entry_regs;

  // Save ss:sp and switch to our internal stack. We also save bp
  // so that we can get at any parameter at the top of the stack
  // (such as the file attribute passed to subfxn 17h).
  _asm mov dos_ss, ss;
  _asm mov save_bp, bp;

  stack_param_ptr = (uint16_t far *) MK_FP(dos_ss, save_bp + sizeof(ALL_REGS));

  cur_ss = getSS();
  cur_sp = getSP();
  our_sp = (FP_OFF(our_stack) + 15) >> 4;
  our_ss = FP_SEG(our_stack) + our_sp;
  our_sp = STACK_SIZE - 2 - (((our_sp - (FP_OFF(our_stack) >> 4)) << 4)
                             - (FP_OFF(our_stack) & 0xf));

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

  // Expect success!
  succeed();

#if defined(DEBUG_DISPATCH) && defined(DEBUG)
  consolef("DISPATCH IN 0x%02x\n", curr_fxn);
#endif
  // Call the appropriate handling function unless we already know we
  // need to fail
  if (filename_is_char_device)
    fail(DOSERR_ACCESS_DENIED);
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

  // put the possibly changed registers back on the stack, and return
  entry_regs = r;
  return;

  // If the call wasn't for us, we chain on.
 chain_on:
  _chain_intr(prev_int2f_vector);
}
