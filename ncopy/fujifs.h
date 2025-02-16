/* Contributed by fozztexx@fozztexx.com
 */

#ifndef _NET_H
#define _NET_H

#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>

typedef int errcode;
typedef struct {
  const char *name;
  off_t size;
  time_t ctime, mtime;
  unsigned char isdir:1;
} FN_DIRENT;

enum {
  FUJIFS_READ                     = 4,
  FUJIFS_DIRECTORY                = 6,
  FUJIFS_WRITE                    = 8,
};

// FIXME - this should probably return a handle to point to the network device which was used?
extern errcode fujifs_open_url(const char *url, const char *user, const char *password);
extern errcode fujifs_close_url();
extern errcode fujifs_open(const char *path, uint16_t mode);
extern errcode fujifs_close();
extern size_t fujifs_read(uint8_t *buf, size_t length);
extern size_t fujifs_write(uint8_t *buf, size_t length);
extern errcode fujifs_opendir();
extern errcode fujifs_closedir();
extern FN_DIRENT *fujifs_readdir();
extern errcode fujifs_chdir();

#endif /* _NET_H */
