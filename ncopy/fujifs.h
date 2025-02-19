/* Contributed by fozztexx@fozztexx.com
 */

#ifndef _FUJIFS_H
#define _FUJIFS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h> // for off_t
#include <fcntl.h>
#include <time.h>

#if __WATCOMC__ < 1300
#define strcasecmp stricmp
#endif

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

#endif /* _FUJIFS_H */
