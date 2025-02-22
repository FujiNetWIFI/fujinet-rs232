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

// FIXME - find available network device
#define NETDEV DEVICEID_FN_NETWORK
#define OPEN_SIZE 256
#define DIR_DELIM " \r\n"

struct {
  unsigned short length;
  unsigned char connected;
  unsigned char errcode;
} status;

typedef struct {
  size_t position, length;
} FN_DIR;
  
static uint8_t fujifs_buf[OPEN_SIZE];
static FN_DIR cur_dir;

// Copy path to fujifs_buf and make sure it has N: prefix
void ennify(const char far *path)
{
  uint16_t idx, len, remain;


  idx = 0;
  // FIXME - handle N1: N2: etc.
  if (toupper(path[0]) != 'N' || path[1] != ':') {
    memcpy(fujifs_buf, "N:", 2);
    idx = 2;
  }

  len = _fstrlen(path);
  remain = sizeof(fujifs_buf) - idx - 1;
  if (len > remain)
    len = remain;
  _fmemmove(&fujifs_buf[idx], path, len);
  fujifs_buf[idx + len] = 0;
  return;
}
  
errcode fujifs_open_url(const char *url, const char *user, const char *password)
{
  int reply;


  // User/pass is "sticky" and needs to be set/reset on open
  memset(fujifs_buf, 0, sizeof(fujifs_buf));
  if (user)
    strcpy(fujifs_buf, user);
  reply = fujiF5_write(NETDEV, CMD_USERNAME, 0, 0, &fujifs_buf, OPEN_SIZE);
  // FIXME - check err
  memset(fujifs_buf, 0, sizeof(fujifs_buf));
  if (password)
    strcpy(fujifs_buf, password);
  reply = fujiF5_write(NETDEV, CMD_PASSWORD, 0, 0, &fujifs_buf, OPEN_SIZE);
  // FIXME - check err
  
  return fujifs_open(url, FUJIFS_DIRECTORY);
}

errcode fujifs_close_url()
{
  return fujifs_close();
}

errcode fujifs_open(const char far *path, uint16_t mode)
{
  int reply;


  ennify(path);
  reply = fujiF5_write(NETDEV, CMD_OPEN, mode, 0, &fujifs_buf, OPEN_SIZE);
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_OPEN OPEN REPLY: 0x%02x\n", reply);
  // FIXME - check err

  reply = fujiF5_read(NETDEV, CMD_STATUS, 0, 0, &status, sizeof(status));
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_OPEN STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err

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

errcode fujifs_close()
{
  fujiF5_none(NETDEV, CMD_CLOSE, 0, 0, NULL, 0);
  return 0;
}

// Returns number of bytes read
size_t fujifs_read(uint8_t far *buf, size_t length)
{
  int reply;


  // Check how many bytes are available
  reply = fujiF5_read(NETDEV, CMD_STATUS, 0, 0, &status, sizeof(status));
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_READ STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err

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
size_t fujifs_write(uint8_t far *buf, size_t length)
{
  int reply;


  reply = fujiF5_write(DEVICEID_FN_NETWORK, CMD_WRITE, length, 0, buf, length);
  if (reply != REPLY_COMPLETE)
    return 0;
  return length;
}

size_t fujifs_tell()
{
  int reply;


  // Check how many bytes are unread
  reply = fujiF5_read(NETDEV, CMD_STATUS, 0, 0, &status, sizeof(status));
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_TELL STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err

  if ((status.errcode > NETWORK_SUCCESS && !status.length)
      /* || !status.connected // status.connected doesn't work */)
    return 0;
  return status.length;
}

errcode fujifs_opendir(const char far *path)
{
  errcode err;
  uint16_t len;


  /* FIXME - FujiNet seems to open in directory mode even if it's a
             file, so append "/." to make it respect directory mode. */
  ennify(path);
  len = strlen(fujifs_buf);
  if (fujifs_buf[len - 1] == '/')
    fujifs_buf[len - 1] = 0;
  strcat(fujifs_buf, "/.");

  cur_dir.position = cur_dir.length = 0;
  return fujifs_open(fujifs_buf, FUJIFS_DIRECTORY);
}

errcode fujifs_closedir()
{
  fujiF5_none(NETDEV, CMD_CLOSE, 0, 0, NULL, 0);
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

FN_DIRENT *fujifs_readdir()
{
  size_t len;
  static FN_DIRENT ent;
  size_t idx;
  char *cptr1, *cptr2, *cptr3;
  int len1, len2;


  // Refill buffer if it's empty
  if (cur_dir.position >= cur_dir.length) {
    cur_dir.length = fujifs_read(fujifs_buf, sizeof(fujifs_buf));
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
    len2 = fujifs_read(&fujifs_buf[len1], sizeof(fujifs_buf) - len1);
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


  ennify(path);
  reply = fujiF5_write(NETDEV, CMD_CHDIR, 0x0000, 0, &fujifs_buf, OPEN_SIZE);
  if (reply != REPLY_COMPLETE)
    printf("FUJIFS_CHDIR CHDIR REPLY: 0x%02x\n", reply);
  // FIXME - check err
  return 0;
}
