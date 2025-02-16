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
  
static uint8_t fujifs_buf[256];
static FN_DIR cur_dir;
static struct tm ftm;

// Copy path to fujifs_buf and make sure it has N: prefix
void ennify(const char *path)
{
  fujifs_buf[0] = 0;
  if (toupper(path[0]) != 'N' || path[1] != ':')
    strcat(fujifs_buf, "N:");
  strncat(fujifs_buf, path, OPEN_SIZE - 1 - strlen(fujifs_buf));
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

errcode fujifs_open(const char *path, uint16_t mode)
{
  int reply;


  ennify(path);
  reply = fujiF5_write(NETDEV, CMD_OPEN, mode, 0, &fujifs_buf, OPEN_SIZE);
  if (reply != REPLY_COMPLETE)
    printf("FN OPEN REPLY: 0x%02x\n", reply);
  // FIXME - check err

  reply = fujiF5_read(NETDEV, CMD_STATUS, 0, 0, &status, sizeof(status));
  if (reply != REPLY_COMPLETE)
    printf("FN STATUS REPLY: 0x%02x\n", reply);
  // FIXME - check err

#if 0
  printf("FN STATUS: len %i  con %i  err %i\n",
	 status.length, status.connected, status.errcode);
#endif
  // FIXME - apparently the error returned when opening in write mode should be ignored?
  if (mode == FUJIFS_WRITE)
    return 0;

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
size_t fujifs_read(uint8_t *buf, size_t length)
{
  int reply;


  // Check how many bytes are available
  reply = fujiF5_read(NETDEV, CMD_STATUS, 0, 0, &status, sizeof(status));
  if (reply != REPLY_COMPLETE)
    printf("FN STATUS REPLY: 0x%02x\n", reply);
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
size_t fujifs_write(uint8_t *buf, size_t length)
{
  int reply;


  reply = fujiF5_write(DEVICEID_FN_NETWORK, CMD_WRITE, length, 0, buf, length);
  if (reply != REPLY_COMPLETE)
    return 0;
  return length;
}

errcode fujifs_opendir()
{
  errcode err;


  cur_dir.position = cur_dir.length = 0;
  return fujifs_open("", FUJIFS_DIRECTORY);
}

errcode fujifs_closedir()
{
  fujiF5_none(NETDEV, CMD_CLOSE, 0, 0, NULL, 0);
  return 0;
}

FN_DIRENT *fujifs_readdir()
{
  size_t len;
  static FN_DIRENT ent;
  size_t idx;
  char *cptr1, *cptr2, *cptr3;
  int v1, v2, v3;


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
    v1 = cur_dir.length - cur_dir.position;
    memmove(fujifs_buf, &fujifs_buf[cur_dir.position], v1);
    v2 = fujifs_read(&fujifs_buf[v1], sizeof(fujifs_buf) - v1);
    if (!v2)
      return NULL;
    cur_dir.position = 0;
    cur_dir.length = v1 + v2;
  }

  memset(&ent, 0, sizeof(ent));

  // get filename
  cptr1 = strtok(&fujifs_buf[cur_dir.position], DIR_DELIM);
  ent.name = cptr1;

  // get extension
  cptr2 = strtok(NULL, DIR_DELIM);
  if (cptr2 - cptr1 < 10) {
    v1 = strlen(cptr1);
    cptr1[v1] = '.';
    memmove(&cptr1[v1 + 1], cptr2, strlen(cptr2) + 1);

    // get size or dir
    cptr1 = strtok(NULL, DIR_DELIM);
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
  cptr1 = strtok(NULL, DIR_DELIM);

  // get time
  cptr2 = strtok(NULL, DIR_DELIM);

  // done parsing record, parse date & time now
  cptr3 = strtok(cptr1, "-");
  ftm.tm_mon = atoi(cptr3) - 1;
  cptr3 = strtok(NULL, "-");
  ftm.tm_mday = atoi(cptr3);
  cptr3 = strtok(NULL, "-");
  ftm.tm_year = atoi(cptr3) + 1900;
  if (ftm.tm_year < 1975)
    ftm.tm_year += 100;
  ftm.tm_year -= 1900;

  cptr3 = strtok(cptr2, ":");
  ftm.tm_hour = atoi(cptr3);
  cptr3 = strtok(NULL, " ");
  ftm.tm_min = atoi(cptr3);
  ftm.tm_hour = ftm.tm_hour % 12 + (tolower(cptr3[2]) == 'p' ? 12 : 0);

  ent.mtime = mktime(&ftm);

  v1 = (cptr3 - fujifs_buf) + 4;
  cur_dir.position = v1;

  return &ent;
}

errcode fujifs_chdir(const char *path)
{
  int reply;


  ennify(path);
  reply = fujiF5_write(NETDEV, CMD_CHDIR, 0x0000, 0, &fujifs_buf, OPEN_SIZE);
  if (reply != REPLY_COMPLETE)
    printf("FN OPEN REPLY: 0x%02x\n", reply);
  // FIXME - check err
  return 0;
}
