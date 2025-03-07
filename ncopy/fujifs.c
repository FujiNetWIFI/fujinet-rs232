/* Contributed by fozztexx@fozztexx.com
 */

#include "fujifs.h"
#include "fujicom.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <dos.h>
#include <stdlib.h>

#include "../sys/print.h"

#undef NETDEV_NEEDS_DIGIT

// FIXME - find available network device
#define NETDEV(x)       (DEVICEID_FN_NETWORK + x - 1)
#define NETDEV_TOTAL    (DEVICEID_FN_NETWORK_LAST - DEVICEID_FN_NETWORK + 1)
#define FN_HANDLE(x)    (fujifs_open_handles[(x) - 1])
#ifdef NETDEV_NEEDS_DIGIT
#define NETDEV_PREFIX   "N0:"
#else
#define NETDEV_PREFIX   "N:"
#endif
#define OPEN_SIZE       256
#define DIR_DELIM       " \r\n"

#define ATARI_STRING_TERM 0x9B

struct {
  unsigned short length;
  unsigned char connected;
  unsigned char errcode;
} status;

typedef struct {
  uint8_t parent;
  uint8_t is_open:1;
  uint8_t did_auth:1;
  size_t position, length;
  char user[32], password[32];
} fn_network_handle;

static fn_network_handle fujifs_open_handles[NETDEV_TOTAL];
static uint8_t fujifs_buf[OPEN_SIZE];
static char fujifs_did_init = 0;

// Copy path to fujifs_buf and make sure it has N: prefix
void ennify(int devnum, const char far *path)
{
  uint16_t idx, len, remain;
  int has_prefix;


  idx = 0;
#ifdef NETDEV_NEEDS_DIGIT
  has_prefix = toupper(path[0]) == 'N'
    && ((path[1] == ':' && devnum == 1)
        || (path[2] == ':' && path[1] == '0' + devnum));
  if (!has_prefix) {
    idx = sizeof(NETDEV_PREFIX) - 1;
    memcpy(fujifs_buf, NETDEV_PREFIX, idx);
    fujifs_buf[1] = '0' + devnum;
  }
#else
  has_prefix = toupper(path[0]) == 'N' && path[1] == ':';
  if (!has_prefix) {
    idx = sizeof(NETDEV_PREFIX) - 1;
    memcpy(fujifs_buf, NETDEV_PREFIX, idx);
  }
#endif

  len = _fstrlen(path);
  remain = sizeof(fujifs_buf) - idx - 1;
  if (len > remain)
    len = remain;
  _fmemmove(&fujifs_buf[idx], path, len);
  fujifs_buf[idx + len] = 0;
  return;
}

fujifs_handle fujifs_find_handle()
{
  int idx;


  if (!fujifs_did_init) {
    memset(fujifs_open_handles, 0, sizeof(fujifs_open_handles));
    fujifs_did_init = 1;
  }

  for (idx = 0; idx < NETDEV_TOTAL; idx++) {
    if (!FN_HANDLE(idx + 1).is_open) {
      FN_HANDLE(idx + 1).is_open = 1;
      return idx + 1;
    }
  }

  return 0;
}

errcode fujifs_open_url(fujifs_handle far *host_handle, const char *url,
                        const char *user, const char *password)
{
  int reply;
  fujifs_handle temp;
  errcode err;
  fn_network_handle *hhp;


  temp = fujifs_find_handle();
  if (!temp)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  hhp = &FN_HANDLE(temp);
  if (user)
    strcpy(hhp->user, user);
  if (password)
    strcpy(hhp->password, password);

  // User/pass is "sticky" and needs to be set/reset on open
  reply = fujiF5_write(NETDEV(temp), CMD_USERNAME, 0, 0, hhp->user, OPEN_SIZE);
  // FIXME - check err
  reply = fujiF5_write(NETDEV(temp), CMD_PASSWORD, 0, 0, hhp->password, OPEN_SIZE);
  // FIXME - check err

  // This wasn't an open commend so no need to close, just mark it
  // available, it'll get re-used on the real open
  hhp->is_open = 0;

  err = fujifs_open(0, host_handle, url, FUJIFS_DIRECTORY);
  if (err)
    return err;

  // Tell FujiNet to remember it was open
  fujifs_chdir(*host_handle, url);
  FN_HANDLE(*host_handle).parent = *host_handle;

  return err;
}

errcode fujifs_close_url(fujifs_handle handle)
{
  return fujifs_close(handle);
}

static uint8_t is_fq_url(const char far *path)
{
  uint16_t idx;


  for (idx = 0; path[idx] && path[idx+1] && path[idx+2]; idx++)
    if (path[idx] == ':' && path[idx+1] == '/' && path[idx+2] == '/')
      return 1;

  return 0;
}

errcode fujifs_open(fujifs_handle host_handle, fujifs_handle far *file_handle,
                    const char far *path, uint16_t mode)
{
  int reply;
  fn_network_handle *fhp;


  *file_handle = fujifs_find_handle();
  if (!*file_handle)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;
  fhp = &FN_HANDLE(*file_handle);

  if (host_handle) {
    fn_network_handle *hhp = &FN_HANDLE(host_handle);


    if (hhp->user[0] && (!fhp->did_auth || host_handle != fhp->parent)) {
      reply = fujiF5_write(NETDEV(*file_handle), CMD_USERNAME, 0, 0, hhp->user, OPEN_SIZE);
      // FIXME - check err
      reply = fujiF5_write(NETDEV(*file_handle), CMD_PASSWORD, 0, 0, hhp->password, OPEN_SIZE);
      // FIXME - check err
      fhp->did_auth = 1;
    }

    if (host_handle != fhp->parent && !is_fq_url(path)) {
      int idx;


      // Get prefix of parent
      reply = fujiF5_read(NETDEV(host_handle), CMD_GETCWD, 0, 0, fujifs_buf, OPEN_SIZE);
      if (reply != REPLY_COMPLETE) {
        return NETWORK_ERROR_SERVICE_NOT_AVAILABLE;
      }
      for (idx = 0; idx < sizeof(fujifs_buf) - 1 && fujifs_buf[idx]
             && fujifs_buf[idx] != ATARI_STRING_TERM; idx++)
        ;
      fujifs_buf[idx] = 0;
      // FIXME - check err

      // Set prefix of new handle
      reply = fujiF5_write(NETDEV(*file_handle), CMD_CHDIR, 0, 0, fujifs_buf, OPEN_SIZE);
      // FIXME - check err
    }


    fhp->parent = host_handle;
  }

  ennify(*file_handle, path);
  reply = fujiF5_write(NETDEV(*file_handle), CMD_OPEN, mode, 0, fujifs_buf, OPEN_SIZE);
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_OPEN OPEN REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

  reply = fujiF5_read(NETDEV(*file_handle), CMD_STATUS, 0, 0, &status, sizeof(status));
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_OPEN STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

#if 0
  printf("FN STATUS: len %i  con %i  err %i\n",
         status.length, status.connected, status.errcode);
#endif
  // FIXME - apparently the error returned when opening in write mode should be ignored?
  if (mode == FUJIFS_WRITE)
    goto done;

  /* We haven't even read the file yet, it's not EOF */
  if (status.errcode == NETWORK_ERROR_END_OF_FILE)
    status.errcode = NETWORK_SUCCESS;

  if (status.errcode > NETWORK_SUCCESS && !status.length) {
    fhp->is_open = 0;
    return status.errcode;
  }

#if 0
  // FIXME - field doesn't work
  if (!status.connected)
    return -1;
#endif

 done:
  fhp->parent = host_handle;
  return 0;
}

errcode fujifs_close(fujifs_handle handle)
{
  if (handle < 1 || handle > NETDEV_TOTAL || !FN_HANDLE(handle).is_open)
    return NETWORK_ERROR_NOT_CONNECTED;

  fujiF5_none(NETDEV(handle), CMD_CLOSE, 0, 0, NULL, 0);
  FN_HANDLE(handle).is_open = 0;
  return 0;
}

// Returns number of bytes read
size_t fujifs_read(fujifs_handle handle, uint8_t far *buf, size_t length)
{
  int reply;


  if (handle < 1 || handle > NETDEV_TOTAL || !FN_HANDLE(handle).is_open)
    return 0;

  // Check how many bytes are available
  reply = fujiF5_read(NETDEV(handle), CMD_STATUS, 0, 0, &status, sizeof(status));
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_READ STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

#if 0
  printf("FN STATUS: len %i  con %i  err %i\n",
         status.length, status.connected, status.errcode);
#endif
  if ((status.errcode > NETWORK_SUCCESS && !status.length)
      /* || !status.connected // status.connected doesn't work */)
    return 0;

  if (length > status.length)
    length = status.length;

  reply = fujiF5_read(NETDEV(handle), CMD_READ, length, 0, buf, length);
  if (reply != REPLY_COMPLETE)
    return 0;
  return length;
}

// Returns number of bytes written
size_t fujifs_write(fujifs_handle handle, uint8_t far *buf, size_t length)
{
  int reply;


  if (handle < 1 || handle >= NETDEV_TOTAL)
    return 0;

  reply = fujiF5_write(NETDEV(handle), CMD_WRITE, length, 0, buf, length);
  if (reply != REPLY_COMPLETE)
    return 0;
  return length;
}

errcode fujifs_opendir(fujifs_handle host_handle, fujifs_handle far *dir_handle,
                       const char far *path)
{
  errcode err;
  uint16_t len;
  fujifs_handle temp;
  char *sep;


  /* FIXME - FujiNet seems to open in directory mode even if it's a
             file, so append "/." to make it respect directory mode. */

  // Figure out which N: device will be used and add prefix so
  // fujifs_buf doesn't get modified during open
  temp = fujifs_find_handle();
  if (!temp)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  ennify(temp, path);
  FN_HANDLE(temp).is_open = 0;

  sep = strchr(fujifs_buf, ':');
  if (*(sep + 1)) {
    len = strlen(fujifs_buf);
    if (fujifs_buf[len - 1] == '/')
      fujifs_buf[len - 1] = 0;
    strcat(fujifs_buf, "/.");
  }

  FN_HANDLE(temp).position = FN_HANDLE(temp).length = 0;
  // FIXME - check if open failed and return NETWORK_ERROR_NOT_A_DIRECTORY
  return fujifs_open(host_handle, dir_handle, fujifs_buf, FUJIFS_DIRECTORY);
}

errcode fujifs_closedir(fujifs_handle handle)
{
  fujifs_close(handle);
  return 0;
}

/* Open Watcom strtok doesn't seem to work in an interrupt */
char *fujifs_strtok(char *str, const char *delim)
{
  static char *last;
  int idx;


  if (!str)
    str = last;

  /* Skip over any leading characters in delim */
  for (; *str; str++) {
    for (idx = 0; delim[idx]; idx++)
      if (*str == delim[idx])
        break;
    if (!delim[idx])
      break;
  }

  /* Find next delim */
  for (last = str; *last; last++) {
    for (idx = 0; delim[idx]; idx++)
      if (*last == delim[idx])
        break;
    if (delim[idx])
      break;
  }

  *last = 0;
  last++;
  return str;
}

FN_DIRENT *fujifs_readdir(fujifs_handle handle)
{
  size_t len;
  static FN_DIRENT ent;
  size_t idx;
  char *cptr1, *cptr2, *cptr3;
  int len1, len2;


  // Refill buffer if it's empty
  if (FN_HANDLE(handle).position >= FN_HANDLE(handle).length) {
    FN_HANDLE(handle).length = fujifs_read(handle, fujifs_buf,
                                                         sizeof(fujifs_buf));
    if (!FN_HANDLE(handle).length)
      return NULL;
    FN_HANDLE(handle).position = 0;
  }

  for (idx = FN_HANDLE(handle).position;
       idx < FN_HANDLE(handle).length &&
         (fujifs_buf[idx] == ' ' || fujifs_buf[idx] == '\r' || fujifs_buf[idx] == '\n');
       idx++)
    ;
  FN_HANDLE(handle).position = idx;

  // make sure there's an END-OF-RECORD, if not refill buffer
  for (; idx < FN_HANDLE(handle).length
         && fujifs_buf[idx] != '\r' && fujifs_buf[idx] != '\n';
       idx++)
    ;
  if (idx == FN_HANDLE(handle).length) {
    len1 = FN_HANDLE(handle).length - FN_HANDLE(handle).position;
    memmove(fujifs_buf, &fujifs_buf[FN_HANDLE(handle).position], len1);
    len2 = fujifs_read(handle, &fujifs_buf[len1], sizeof(fujifs_buf) - len1);
    if (!len2)
      return NULL;
    if (!len2)
      return NULL;
    FN_HANDLE(handle).position = 0;
    FN_HANDLE(handle).length = len1 + len2;
  }

  memset(&ent, 0, sizeof(ent));

  // get filename
  cptr1 = fujifs_strtok(&fujifs_buf[FN_HANDLE(handle).position], DIR_DELIM);
  ent.name = cptr1;

  // get extension
  cptr2 = fujifs_strtok(NULL, DIR_DELIM);
  if (cptr2 - cptr1 < 10) {
    len1 = strlen(cptr1);
    cptr1[len1] = '.';
    memmove(&cptr1[len1 + 1], cptr2, strlen(cptr2) + 1);

    // get size or dir
    cptr1 = fujifs_strtok(NULL, DIR_DELIM);
  }
  else {
    // extension is too far away, it must be the size
    cptr1 = cptr2;
  }

  if (strcasecmp(cptr1, "<DIR>") == 0)
    ent.isdir = 1;
  else
    ent.size = atol(cptr1);

  // get date
  cptr1 = fujifs_strtok(NULL, DIR_DELIM);

  // get time
  cptr2 = fujifs_strtok(NULL, DIR_DELIM);

  // done parsing record, parse date & time now
  cptr3 = fujifs_strtok(cptr1, "-");
  ent.mtime.tm_mon = atoi(cptr3) - 1;
  cptr3 = fujifs_strtok(NULL, "-");
  ent.mtime.tm_mday = atoi(cptr3);
  cptr3 = fujifs_strtok(NULL, "-");
  ent.mtime.tm_year = atoi(cptr3) + 1900;
  if (ent.mtime.tm_year < 1975)
    ent.mtime.tm_year += 100;
  ent.mtime.tm_year -= 1900;

  cptr3 = fujifs_strtok(cptr2, ":");
  ent.mtime.tm_hour = atoi(cptr3);
  cptr3 += strlen(cptr3) + 1;
  ent.mtime.tm_min = atoi(cptr3);
  ent.mtime.tm_hour = ent.mtime.tm_hour % 12 + (tolower(cptr3[2]) == 'p' ? 12 : 0);

  len1 = (cptr3 - fujifs_buf) + 4;
  FN_HANDLE(handle).position = len1;

  return &ent;
}

errcode fujifs_chdir(fujifs_handle host_handle, const char far *path)
{
  int reply;
  int idx;


  ennify(host_handle, path);
  reply = fujiF5_write(NETDEV(host_handle), CMD_CHDIR, 0, 0, fujifs_buf, OPEN_SIZE);
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_CHDIR CHDIR REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

  // Invalidate all other network drives that have us as parent
  for (idx = 0; idx < NETDEV_TOTAL; idx++)
    if (FN_HANDLE(idx + 1).parent == host_handle)
      FN_HANDLE(idx + 1).parent = 0;
  FN_HANDLE(host_handle).parent = host_handle;

  return 0;
}

errcode fujifs_seek(fujifs_handle handle, off_t position)
{
  int reply;


  if (handle < 1 || handle > NETDEV_TOTAL || !FN_HANDLE(handle).is_open)
    return NETWORK_ERROR_NOT_CONNECTED;

  reply = fujiF5_write(NETDEV(handle), CMD_SEEK,
		       position & 0xffff, (position >> 16) & 0xffff, NULL, 0);
  if (reply != REPLY_COMPLETE)
    return NETWORK_ERROR_SERVICE_NOT_AVAILABLE;

  return NETWORK_SUCCESS;
}
