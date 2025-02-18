#include "netredir.h"
#include "dosdata.h"
#include "../ncopy/fujifs.h"
#include <dos.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h> // strcasecmp
#include <ctype.h>
#include <conio.h>
#include "../sys/print.h"

//#pragma data_seg("BEGTEXT", "CODE");
//#pragma code_seg("BEGTEXT", "CODE");

//#pragma code_seg("_INIT", "INIT")
static union REGS regs;
struct SREGS segr;

char buf[256];

void get_password(char *password, size_t max_len);

int main(int argc, char *argv[])
{
  uint16_t end;
  LOLREC_PTR lolptr;              /* pointer to List Of Lists */
  int drive_letter;
  const char *url;
  errcode err;


  if (argc < 2) {
    printf("Usage: %s <command>\n", argv[0]);
    exit(1);
  }

  // Only support map command at this time
  if (strcasecmp(argv[1], "map") != 0 || argc < 4) {
    printf("Usage: %s map L: <url_of_share>\n", argv[0]);
    exit(1);
  }

  drive_letter = toupper(argv[2][0]);
  drive_num = drive_letter - 'A';
  url = argv[3];

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

  // FIXME - allocate drive letter
  regs.x.ax = 0x5D06;
  intdosx(&regs, &regs, &segr);
  sda_ptr = MK_FP(segr.ds, regs.x.si);
  printf("SDA: 0x%08lx\n", (uint32_t) sda_ptr);

  regs.x.ax = 0x5200;
  intdosx(&regs, &regs, &segr);
  lolptr = (LOLREC_PTR) MK_FP(segr.es, regs.x.bx);
  printf("LOL: 0x%08lx\n", (uint32_t) lolptr);
  printf("CDS: 0x%08lx\n", (uint32_t) lolptr->cds_ptr);

  {
    CDS_PTR_V3 our_cds_ptr;
    uint16_t cds_root_size;             /* Size of our CDS root string */
    char far *current_path;         /* ptr to current path in CDS */
    char far *cds_path_root = "FujiNet  :\\";       /* Root string for CDS */


    our_cds_ptr = lolptr->cds_ptr;
    if (_osmajor == 3)
      our_cds_ptr = our_cds_ptr + drive_num;
    else {
      CDS_PTR_V4 t = (CDS_PTR_V4) our_cds_ptr;


      t = t + drive_num;
      our_cds_ptr = (CDS_PTR_V3) t;
    }

    printf("CDS data\n");
    dumpHex(lolptr, sizeof(*lolptr), 0);

    if (drive_num >= lolptr->last_drive) {
      printf("Drive letter %c higher than last drive %c",
             drive_num + 'A', lolptr->last_drive + 'A');
      exit(1);
    }

    // Check that this drive letter is currently invalid (not in use already)
    // 0xc000 tests both physical and network bits at same time
    if ((our_cds_ptr->flags & 0xc000) != 0) {
      printf("Drive already assigned...");
      exit(1);
    }

    // Set Network+Physical+NotRealNetworkDrive bits on, and
    // establish our 'root'
    our_cds_ptr->flags |= 0xc000;
    cds_root_size = _fstrlen(cds_path_root);
    _fstrcpy(our_cds_ptr->current_path, cds_path_root);
    our_cds_ptr->current_path[_fstrlen(our_cds_ptr->current_path) - 3] =
      (char) ('A' + drive_num);
    _fstrcpy(cds_path_root, our_cds_ptr->current_path);
    current_path = our_cds_ptr->current_path;
    our_cds_ptr->root_ofs = _fstrlen(our_cds_ptr->current_path) - 1;
    current_path += our_cds_ptr->root_ofs;
  }

  printf("Installing FujiNet redirector on drive %c:\n", drive_letter);
  old_int2f = _dos_getvect(DOS_INT_REDIR);
  printf("Old vector: 0x%08lx\n", (uint32_t) old_int2f);
  _dos_setvect(DOS_INT_REDIR, redirector);

  // Become a TSR
#if 0
  end = (FP_SEG(main) - getCS()) << 4;
  end += 256; // size of PSP
  end += 15;
  _dos_keep(0, end >> 4);
#else
  {
    void far *heap = sbrk(0);
    uint16_t far *psp_ptr;


    end = (FP_SEG(heap) << 4) + FP_OFF(heap);
    end -= _psp << 4;
    end += 15;

    printf("Heap: 0x%08lx  PSP: 0x%04x\n", (uint32_t) heap, _psp);
    printf("CS: 0x%04x\n", getCS());
    printf("Para: %04x\n", end);
    psp_ptr = MK_FP(_psp, 0);
    printf("Top seg: %04x\n", psp_ptr[1]);
    _dos_keep(0, end >> 4);
  }
#endif

  return 0;
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
