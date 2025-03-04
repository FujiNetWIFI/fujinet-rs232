#ifndef _DOSDATA_H
#define _DOSDATA_H

#include <stdint.h>

#define TRUE			1
#define FALSE			0

#define DOS_MAX_PATHLEN         256
#define DOS_FCBNAME_LEN         11

enum {
  MODE_READONLY         = 0x00,
  MODE_WRITEONLY        = 0x01,
  MODE_READWRITE        = 0x02,
  MODE_CASESENSITVE     = 0x03,
  MODE_DENYALL          = 0x10,
  MODE_DENYWRITE        = 0x20,
  MODE_DENYREAD         = 0x30,
  MODE_DENYNONE         = 0x40,
  MODE_NETWORK          = 0x70,
  MODE_INHERITANCE      = 0x80,
};

enum {
  OPEXT_OPENED          = 1,
  OPEXT_CREATED         = 2,
  OPEXT_TRUNCATED       = 3,
};

/* Macro to get value from DOS structs which automatically handles checking _osmajor */
#define DOS_STRUCT_VALUE(type, var, field) \
  ((_osmajor == 3) ? ((type##_V3)(var))->field : ((type##_V4)(var))->field)

/* Similar to above, but returns pointer to field */
#define DOS_STRUCT_POINTER(type, var, field) \
  ((_osmajor == 3) ? &((type##_V3)(var))->field : &((type##_V4)(var))->field)

enum {
  ATTR_READ_ONLY        = 0x01,
  ATTR_HIDDEN           = 0x02,
  ATTR_SYSTEM           = 0x04,
  ATTR_VOLUME_LABEL     = 0x08,
  ATTR_DIRECTORY        = 0x10,
  ATTR_ARCHIVE          = 0x20,
};

#pragma pack(push, 1)
/* FindFirst/Next data block - ALL DOS VERSIONS */
typedef struct {
  uint8_t drive_num;
  char pattern[11];
  uint8_t attr_mask;
  uint16_t sequence;
  //uint16_t sector;
  uint16_t fndir_handle;
  uint8_t _reserved1[4];
} SRCHREC, far *SRCHREC_PTR;

/* DOS Directory entry for 'found' file - ALL DOS VERSIONS */
typedef struct {
  char fcb_name[11];
  uint8_t attr;
  uint8_t _reserved1[10];
  union {
    struct {
      uint16_t time, date;
    };
    uint32_t datetime;
  };
  uint16_t start_sector;
  uint32_t size;
} DIRREC, far *DIRREC_PTR;

/* Swappable DOS Area - DOS VERSION 3.xx */
typedef struct {
  uint8_t _reserved0[12];
  uint8_t far *current_dta;
  uint8_t _reserved1[30];
  uint8_t dd;
  uint8_t mm;
  uint16_t yy_1980;
  uint8_t _reserved2[96];
  char path1[128];
  char path2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uint8_t _reserved3[81];
  char fcb_name1[11];
  uint8_t _reserved4;
  char fcb_name2[11];
  uint8_t _reserved5[11];
  uint8_t srch_attr;
  uint8_t open_mode;
  uint8_t _reserved6[48];
  uint8_t far *cdsptr;
  uint8_t _reserved7[72];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} SDA_V3, far *SDA_PTR_V3;

/* Swappable DOS Area - DOS VERSION 4.xx */
typedef struct {
  uint8_t _reserved0[12];
  uint8_t far *current_dta;
  uint8_t _reserved1[32];
  uint8_t dd;
  uint8_t mm;
  uint16_t yy_1980;
  uint8_t _reserved2[106];
  char path1[128];
  char path2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uint8_t _reserved3[88];
  char fcb_name[11];
  uint8_t _reserved4;
  char fcb_name_2[11];
  uint8_t _reserved5[11];
  uint8_t srch_attr;
  uint8_t open_mode;
  uint8_t _reserved6[51];
  uint8_t far *cdsptr;
  uint8_t _reserved7[87];
  uint16_t action_2E;
  uint16_t attr_2E;
  uint16_t mode_2E;
  uint8_t _reserved8[29];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} SDA_V4, far *SDA_PTR_V4;

/* DOS Current directory structure - DOS VERSION 3.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t _reserved1[10];
  uint16_t root_ofs;
} CDS_V3, far *CDS_PTR_V3;

/* DOS Current directory structure - DOS VERSION 4.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t _reserved1[10];
  uint16_t root_ofs;
  uint8_t _reserved2[7];
} CDS_V4, far *CDS_PTR_V4;

/* DOS List of lists structure - DOS VERSIONS 3.1 thru 4 */
/* We don't need much of it. */
typedef struct {
  uint8_t _reserved1[22];
  CDS_PTR_V3 cds_ptr;
  uint8_t _reserved2[7];
  uint8_t last_drive;
} LOLREC, far *LOLREC_PTR;

/* DOS System File Table entry - ALL DOS VERSIONS */
// Some of the fields below are defined by the redirector, and differ
// from the SFT normally found under DOS
typedef struct {
  uint16_t handle_count;
  uint16_t open_mode;
  uint8_t attr;
  uint16_t dev_info_word;
  uint8_t far *dev_drvr_ptr;
  //uint16_t start_sector;
  uint16_t fnfile_handle;
  union {
    struct {
      uint16_t time, date;
    };
    uint32_t datetime;
  };
  uint32_t size;
  uint32_t pos;
  uint16_t rel_sector;
  uint16_t abs_sector;
  uint16_t dir_sector;
  uint8_t sequence;
  char fcb_name[11];
} SFTREC, far *SFTREC_PTR;

/* DOS 4.00 and above lock/unlock region structure */
/* see lockfil() below (Thanks to Martin Westermeier.) */
typedef struct {
  uint32_t region_offset;
  uint32_t region_length;
  uint8_t f0[13];
  char file_name[80];           // 80 is a guess
} LOCKREC, far *LOCKREC_PTR;

#pragma pack(pop)

#endif /* _DOSDATA_H */
