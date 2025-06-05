/* Contributed by fozztexx@fozztexx.com
 */

#ifndef _FUJIFS_H
#define _FUJIFS_H

#include <fujicom.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h> // for off_t
#include <fcntl.h>
#include <time.h>

#if __WATCOMC__ < 1300
#define strcasecmp stricmp
#endif

typedef uint8_t fujifs_handle;
typedef int errcode;
typedef struct {
  const char *name;
  off_t size;
  struct tm ctime, mtime;
  unsigned char isdir:1;
} FN_DIRENT;

enum {
  FUJIFS_READ           = 4,
  FUJIFS_DIRECTORY      = 6,
  FUJIFS_WRITE          = 8,
  FUJIFS_APPEND         = 9,
  FUJIFS_READWRITE      = 12,
};

extern errcode fujifs_open_url(fujifs_handle far *host_handle, const char *url,
			       const char *user, const char *password);
extern errcode fujifs_close_url(fujifs_handle host_handle);
extern errcode fujifs_open(fujifs_handle host_handle, fujifs_handle far *file_handle,
			   const char far *path, uint16_t mode);
extern errcode fujifs_close(fujifs_handle handle);
extern size_t fujifs_read(fujifs_handle handle, uint8_t far *buf, size_t length);
extern size_t fujifs_write(fujifs_handle handle, uint8_t far *buf, size_t length);
extern errcode fujifs_opendir(fujifs_handle host_handle, fujifs_handle far *dir_handle,
			      const char far *path);
extern errcode fujifs_closedir(fujifs_handle handle);
extern FN_DIRENT *fujifs_readdir(fujifs_handle handle);
extern errcode fujifs_chdir(fujifs_handle host_handle, const char far *path);
extern errcode fujifs_seek(fujifs_handle handle, off_t position);
extern errcode fujifs_stat(fujifs_handle host_handle, const char far *path,
			   FN_DIRENT far *entry);
extern errcode fujifs_rmdir(fujifs_handle host_handle, const char far *path);
extern errcode fujifs_mkdir(fujifs_handle host_handle, const char far *path);
extern errcode fujifs_rename(fujifs_handle host_handle, const char far *oldpath,
			     const char far *newpath);
extern errcode fujifs_unlink(fujifs_handle host_handle, const char far *path);

#endif /* _FUJIFS_H */
