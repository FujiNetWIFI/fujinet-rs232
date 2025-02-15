/* FujiNet network file copier
 * Contributed by fozztexx@fozztexx.com
 */

#include "parser.h"
#include "fuji.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

//#include "../sys/print.h" // debug

char buf[256];

void print_dir();

void main(int argc, char *argv[])
{
  char *url = argv[1];
  int err;
  parsed cmd;
  int done = 0;


  if (argc < 2) {
    printf("Usage: %s URL\n", argv[0]);
    exit(1);
  }
  
  err = fuji_open_url(url);
  if (err) {
    printf("Err: %i unable to open URL: %s\n", err, url);
    exit(1);
  }

  // Opened succesfully, we don't need it anymore
  err = fuji_close_url();

  // Tell FujiNet to remember it was open
  fuji_chdir(url);
  
  while (!done) {
    printf("ncopy> ");
    fflush(stdout);
    fgets(buf, sizeof(buf), stdin);
    if (!buf[0] || buf[0] == '\n')
      continue;

    cmd = parse_command(buf);
    switch (cmd.cmd) {
    case CMD_DIR:
      print_dir();
      break;

    case CMD_GET:
      printf("get the file\n");
      break;

    case CMD_PUT:
      printf("put the file\n");
      break;

    case CMD_CD:
      printf("change the directory\n");
      break;

    case CMD_EXIT:
      done = 1;
      break;

    default:
      printf("Unrecognized command\n");
      break;
    }
  }
  
  exit(0);
}

void print_dir()
{
  errcode err;
  size_t len;
  FN_DIRENT *ent;


  err = fuji_opendir();
  if (err) {
    printf("Unable to read directory\n");
    return;
  }

  printf("Reading dir\n");

#if 1
  while ((ent = fuji_readdir())) {
    printf("%s %li\n", ent->name, ent->size);
  }
#else
  for (;;) {
    len = fuji_read(buf, sizeof(buf));
    printf("DIR READ: %\n", len);
    if (!len)
      break;
    printf("%.*s", sizeof(buf), buf);
  }
#endif
  printf("\n");

  fuji_closedir();

  return;
}
