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

// FIXME - this should probably return a handle to point to the network device which was used?
extern errcode fuji_open_url(const char *url);
extern errcode fuji_close_url();
extern size_t fuji_read(uint8_t *buf, size_t length);
extern errcode fuji_opendir();
extern errcode fuji_closedir();
extern FN_DIRENT *fuji_readdir();
extern errcode fuji_chdir();

#endif /* _NET_H */
