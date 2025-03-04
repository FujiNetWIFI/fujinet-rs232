#ifndef _TYPES_H
#define _TYPES_H

#define         TRUE                            1
#define         FALSE                           0

/* ****************************************************
   Basic typedefs
   **************************************************** */

typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned long ulong;

typedef int (far *FARPROC)(void);

/* ****************************************************
   Typedefs and structures
   **************************************************** */

#pragma pack(1)

/* TSR signature and unload info structure */
typedef struct {
  uchar cmdline_len;
  char signature[10];           /* The TSR's signature string */
  uint psp;                     /* This instance's PSP */
  uchar drive_no;               /* A: is 1, B: is 2, etc. */
  uint xms_handle;              /* This instance's disk XMS handle */
  uchar far *our_handler;       /* This instance's int 2Fh handler */
  uchar far *prev_handler;      /* Previous int 2Fh handler in the chain */
} SIGREC, far *SIGREC_PTR;

/* FindFirst/Next data block - ALL DOS VERSIONS */
typedef struct {
  uchar drive_no;
  char srch_mask[11];
  uchar attr_mask;
  uint dir_entry_no;
  uint dir_sector;
  uchar f1[4];
} SRCHREC, far *SRCHREC_PTR;

/* DOS System File Table entry - ALL DOS VERSIONS */
// Some of the fields below are defined by the redirector, and differ
// from the SFT normally found under DOS
typedef struct {
  uint handle_count;
  uint open_mode;
  uchar file_attr;
  uint dev_info_word;
  uchar far *dev_drvr_ptr;
  uint start_sector;
  //uint fn_handle;
  ulong file_time;
  long file_size;
  long file_pos;
  uint rel_sector;
  uint abs_sector;
  uint dir_sector;
  uchar dir_entry_no;
  char file_name[11];
} SFTREC, far *SFTREC_PTR;

/* DOS Current directory structure - DOS VERSION 3.xx */
typedef struct {
  char current_path[67];
  uint flags;
  uchar f1[10];
  uint root_ofs;
} V3_CDS, far *V3_CDS_PTR;

/* DOS Current directory structure - DOS VERSION 4.xx */
typedef struct {
  char current_path[67];
  uint flags;
  uchar f1[10];
  uint root_ofs;
  uchar f2[7];
} V4_CDS, far *V4_CDS_PTR;

/* DOS Directory entry for 'found' file - ALL DOS VERSIONS */
typedef struct {
  char file_name[11];
  uchar file_attr;
  uchar f1[10];
  ulong file_time;
  uint start_sector;
  long file_size;
} DIRREC, far *DIRREC_PTR;

/* Swappable DOS Area - DOS VERSION 3.xx */

typedef struct {
  uchar f0[12];
  uchar far *current_dta;
  uchar f1[30];
  uchar dd;
  uchar mm;
  uint yy_1980;
  uchar f2[96];
  char file_name[128];
  char file_name_2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uchar f3[81];
  char fcb_name[11];
  uchar f4;
  char fcb_name_2[11];
  uchar f5[11];
  uchar srch_attr;
  uchar open_mode;
  uchar f6[48];
  uchar far *cdsptr;
  uchar f7[72];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} V3_SDA, far *V3_SDA_PTR;

/* Swappable DOS Area - DOS VERSION 4.xx */
typedef struct {
  uchar f0[12];
  uchar far *current_dta;
  uchar f1[32];
  uchar dd;
  uchar mm;
  uint yy_1980;
  uchar f2[106];
  char file_name[128];
  char file_name_2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uchar f3[88];
  char fcb_name[11];
  uchar f4;
  char fcb_name_2[11];
  uchar f5[11];
  uchar srch_attr;
  uchar open_mode;
  uchar f6[51];
  uchar far *cdsptr;
  uchar f7[87];
  uint action_2E;
  uint attr_2E;
  uint mode_2E;
  uchar f8[29];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} V4_SDA, far *V4_SDA_PTR;

/* DOS List of lists structure - DOS VERSIONS 3.1 thru 4 */
/* We don't need much of it. */
typedef struct {
  uchar f1[22];
  V3_CDS_PTR cds_ptr;
  uchar f2[7];
  uchar last_drive;
} LOLREC, far *LOLREC_PTR;

/* DOS 4.00 and above lock/unlock region structure */
/* see lockfil() below (Thanks to Martin Westermeier.) */
typedef struct {
  ulong region_offset;
  ulong region_length;
  uchar f0[13];
  char file_name[80];           // 80 is a guess
} LOCKREC, far *LOCKREC_PTR;

/* The following structure is compiler specific, and maps
        onto the registers pushed onto the stack for an interrupt
        function. */
typedef struct {
#ifdef __BORLANDC__
  uint bp, di, si, ds, es, dx, cx, bx, ax;
#else
#ifdef __WATCOMC__
  uint gs, fs;
#endif /* __WATCOMC__ */
  uint es, ds, di, si, bp, sp, bx, dx, cx, ax;
#endif
  uint ip, cs, flags;
} ALL_REGS;

#pragma pack()

#endif /* _TYPES_H */
