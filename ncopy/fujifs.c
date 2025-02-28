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
#define NETDEV(x)	(DEVICEID_FN_NETWORK + x - 1)
#define NETDEV_TOTAL    (DEVICEID_FN_NETWORK_LAST - DEVICEID_FN_NETWORK + 1)
#ifdef NETDEV_NEEDS_DIGIT
#define NETDEV_PREFIX	"N0:"
#else
#define NETDEV_PREFIX	"N:"
#endif
#define OPEN_SIZE       256
#define DIR_DELIM       " \r\n"

struct {
  unsigned short length;
  unsigned char connected;
  unsigned char errcode;
} status;

typedef struct {
  size_t position, length;
} FN_DIR;

static uint8_t fujifs_in_use[NETDEV_TOTAL];
static uint8_t fujifs_buf[OPEN_SIZE];
#warning FIXME put cur_dir into handles
static FN_DIR cur_dir;
static char fujifs_did_init = 0;

// Copy path to fujifs_buf and make sure it has N: prefix
void ennify(int devnum, const char far *path)
{
  uint16_t idx, len, remain;
  int has_prefix;


  idx = 0;
#if NETDEV_NEEDS_DIGIT
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
    memset(fujifs_in_use, 0, sizeof(fujifs_in_use));
    fujifs_did_init = 1;
  }

  for (idx = 0; idx < NETDEV_TOTAL; idx++) {
    if (!fujifs_in_use[idx]) {
      fujifs_in_use[idx] = 1;
      return idx + 1;
    }
  }

  return 0;
}

errcode fujifs_open_url(fujifs_handle far *handle, const char *url,
			const char *user, const char *password)
{
  int reply;
  fujifs_handle temp;


  temp = fujifs_find_handle();
  if (!temp)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  // User/pass is "sticky" and needs to be set/reset on open
  memset(fujifs_buf, 0, sizeof(fujifs_buf));

  // FIXME - will CMD_USERNAME or CMD_PASSWORD close or alter open stream?
  if (user)
    strcpy(fujifs_buf, user);
  reply = fujiF5_write(NETDEV(temp), CMD_USERNAME, 0, 0, fujifs_buf, OPEN_SIZE);
  // FIXME - check err

  memset(fujifs_buf, 0, sizeof(fujifs_buf));
  if (password)
    strcpy(fujifs_buf, password);
  reply = fujiF5_write(NETDEV(temp), CMD_PASSWORD, 0, 0, fujifs_buf, OPEN_SIZE);
  // FIXME - check err

  // This wasn't an open commend so no need to close, just mark it available
  fujifs_in_use[temp - 1] = 0;

  return fujifs_open(handle, url, FUJIFS_DIRECTORY);
}

errcode fujifs_close_url(fujifs_handle handle)
{
  return fujifs_close(handle);
}

errcode fujifs_open(fujifs_handle far *handle, const char far *path, uint16_t mode)
{
  int reply;


  *handle = fujifs_find_handle();
  if (!*handle)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  ennify(*handle, path);
  reply = fujiF5_write(NETDEV(*handle), CMD_OPEN, mode, 0, fujifs_buf, OPEN_SIZE);
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_OPEN OPEN REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

  reply = fujiF5_read(NETDEV(*handle), CMD_STATUS, 0, 0, &status, sizeof(status));
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
    return 0;

  /* We haven't even read the file yet, it's not EOF */
  if (status.errcode == NETWORK_ERROR_END_OF_FILE)
    status.errcode = NETWORK_SUCCESS;

  if (status.errcode > NETWORK_SUCCESS && !status.length)
    return status.errcode;

#if 0
  // FIXME - field doesn't work
  if (!status.connected)
    return -1;
#endif

  return 0;
}

errcode fujifs_close(fujifs_handle handle)
{
  if (handle < 1 || handle >= NETDEV_TOTAL || !fujifs_in_use[handle - 1])
    return NETWORK_ERROR_NOT_CONNECTED;

  fujiF5_none(NETDEV(handle), CMD_CLOSE, 0, 0, NULL, 0);
  fujifs_in_use[handle - 1] = 0;
  return 0;
}

// Returns number of bytes read
size_t fujifs_read(fujifs_handle handle, uint8_t far *buf, size_t length)
{
  int reply;


  if (handle < 1 || handle >= NETDEV_TOTAL)
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

  reply = fujiF5_read(DEVICEID_FN_NETWORK, CMD_READ, length, 0, buf, length);
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

errcode fujifs_opendir(fujifs_handle far *handle, const char far *path)
{
  errcode err;
  uint16_t len;
  fujifs_handle temp;


  /* FIXME - FujiNet seems to open in directory mode even if it's a
             file, so append "/." to make it respect directory mode. */

  // Figure out which N: device will be used and stick that prefix on
  temp = fujifs_find_handle();
  if (!temp)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  ennify(temp, path);
  fujifs_in_use[temp - 1] = 0;

  len = strlen(fujifs_buf);
  if (fujifs_buf[len - 1] == '/')
    fujifs_buf[len - 1] = 0;
  strcat(fujifs_buf, "/.");

  cur_dir.position = cur_dir.length = 0;
  // FIXME - check if open failed and return NETWORK_ERROR_NOT_A_DIRECTORY
  return fujifs_open(handle, fujifs_buf, FUJIFS_DIRECTORY);
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
  if (cur_dir.position >= cur_dir.length) {
    cur_dir.length = fujifs_read(handle, fujifs_buf, sizeof(fujifs_buf));
    cur_dir.position = 0;
  }

  for (idx = cur_dir.position;
       idx < cur_dir.length &&
         (fujifs_buf[idx] == ' ' || fujifs_buf[idx] == '\r' || fujifs_buf[idx] == '\n');
       idx++)
    ;
  cur_dir.position = idx;

  // make sure there's an END-OF-RECORD, if not refill buffer
  for (; idx < cur_dir.length && fujifs_buf[idx] != '\r' && fujifs_buf[idx] != '\n';
       idx++)
    ;
  if (idx == cur_dir.length) {
    len1 = cur_dir.length - cur_dir.position;
    memmove(fujifs_buf, &fujifs_buf[cur_dir.position], len1);
    len2 = fujifs_read(handle, &fujifs_buf[len1], sizeof(fujifs_buf) - len1);
    if (!len2)
      return NULL;
    cur_dir.position = 0;
    cur_dir.length = len1 + len2;
  }

  memset(&ent, 0, sizeof(ent));

  // get filename
  cptr1 = fujifs_strtok(&fujifs_buf[cur_dir.position], DIR_DELIM);
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
  cur_dir.position = len1;

  return &ent;
}

errcode fujifs_chdir(const char *path)
{
  int reply;
  fujifs_handle temp;


  temp = fujifs_find_handle();
  if (!temp)
    return NETWORK_ERROR_NO_DEVICE_AVAILABLE;

  ennify(temp, path);
  reply = fujiF5_write(NETDEV(temp), CMD_CHDIR, 0x0000, 0, fujifs_buf, OPEN_SIZE);
#if 0
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_CHDIR CHDIR REPLY: 0x%02x\n", reply);
  // FIXME - check err
#endif

  // FIXME - invalidate all other network drives that have us as parent

  // This wasn't an open commend so no need to close, just mark it available
  fujifs_in_use[temp - 1] = 0;
  return 0;
}
