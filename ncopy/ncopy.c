/* FujiNet network file copier
 * Contributed by fozztexx@fozztexx.com
 */

#include "parser.h"
#include "fujifs.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>

//#include "../sys/print.h" // debug

#define COL_NAME	14
#define COL_SIZE	9

char buf[256];

void print_dir();
void get_file(const char *source, const char *dest);
void put_file(const char *source, const char *dest);
void get_password(char *password, size_t max_len);

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
  
  err = fujifs_open_url(url, NULL, NULL);
  if (err) {
    // Maybe authentication is needed?
    printf("User: ");
    fgets(buf, 128, stdin);
    if (buf[0])
      buf[strlen(buf) - 1] = 0;
    printf("Password: ");
    fflush(stdout);
    get_password(&buf[128], 128);

    err = fujifs_open_url(url, buf, &buf[128]);
    if (err) {
      printf("Err: %i unable to open URL: %s\n", err, url);
      exit(1);
    }
  }

  // Opened succesfully, we don't need it anymore
  err = fujifs_close_url();

  // Tell FujiNet to remember it was open
  fujifs_chdir(url);
  
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
      get_file(cmd.args[1], cmd.args[2]);
      break;

    case CMD_PUT:
      put_file(cmd.args[1], cmd.args[2]);
      break;

    case CMD_CD:
      fujifs_chdir(cmd.args[1]);
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
  struct tm *tm_p;


  err = fujifs_opendir();
  if (err) {
    printf("Unable to read directory\n");
    return;
  }

#if 1
  while ((ent = fujifs_readdir())) {
    tm_p = localtime(&ent->mtime);
    strftime(buf, sizeof(buf) - 1, "%Y-%b-%d %H:%M", tm_p);
    if (ent->isdir)
      printf("%-*s %-*s %s\n", COL_NAME, ent->name, COL_SIZE, "<DIR>", buf);
    else
      printf("%-*s %*lu %s\n", COL_NAME, ent->name, COL_SIZE, ent->size, buf);
  }
#else
  for (;;) {
    len = fujifs_read(buf, sizeof(buf));
    if (!len)
      break;
    printf("%.*s", len, buf);
  }
#endif
  printf("\n");

  fujifs_closedir();

  return;
}

void get_file(const char *source, const char *dest)
{
  FILE *file;
  errcode err;
  size_t len, lenw;
  off_t total;


  if (!dest)
    dest = source;
  file = fopen(dest, "wb");
  if (!file) {
    printf("Failed to open local file: %s\n", dest);
    return;
  }

  err = fujifs_open(source, FUJIFS_READ);
  if (err) {
    printf("Failed to open remote file: %s\n", source);
    fclose(file);
    return;
  }

  total = 0;
  while ((len = fujifs_read(buf, sizeof(buf)))) {
    total += len;
    lenw = fwrite(buf, 1, len, file);
    printf("%10lu bytes transferred.\r", total);
    
    if (lenw != len) {
      printf("Failed to write\n");
      break;
    }
  }
  printf("\n");

  fujifs_close();
  fclose(file);
  return;
}

void put_file(const char *source, const char *dest)
{
  FILE *file;
  errcode err;
  size_t len, lenw;
  off_t total;


  if (!dest)
    dest = source;
  file = fopen(source, "rb");
  if (!file) {
    printf("Failed to open local file: %s\n", source);
    return;
  }

  err = fujifs_open(dest, FUJIFS_WRITE);
  if (err) {
    printf("Failed to open remote file: %s\n", dest);
    fclose(file);
    return;
  }

  total = 0;
  while ((len = fread(buf, 1, sizeof(buf), file))) {
    lenw = fujifs_write(buf, len);
    total += lenw;
    printf("%10lu bytes transferred.\r", total);
    
    if (lenw != len) {
      printf("Failed to write\n");
      break;
    }
  }
  printf("\n");

  fujifs_close();
  fclose(file);
  return;
}

void get_password(char *password, size_t max_len)
{
  size_t idx = 0;
  char ch;


  while (idx < max_len - 1) {
    ch = getch();

    if (ch == '\r' || ch == '\n')
      break;

    // Handle backspace/delete
    if (ch == '\b' || ch == 127) {
      if (idx) {
	// Erase '*' from the screen
        printf("\b \b");
	fflush(stdout);
        idx--;
      }
    }
    else {
      password[idx++] = ch;
      printf("*");
      fflush(stdout);
    }
  }

  password[idx] = 0;
  printf("\n");
  return;
}
