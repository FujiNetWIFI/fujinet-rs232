#ifndef _DOSDATA_H
#define _DOSDATA_H

#include <stdint.h>

/* Macro to get value from DOS structs which automatically handles checking _osmajor */
#define DOS_STRUCT_VALUE(type, var, field) \
  ((_osmajor == 3) ? ((type##_V3)(var))->field : ((type##_V4)(var))->field)

/* Similar to above, but returns pointer to field */
#define DOS_STRUCT_POINTER(type, var, field) \
  ((_osmajor == 3) ? &((type##_V3)(var))->field : &((type##_V4)(var))->field)

#define ATTR_VOLUME_LABEL	0x08

#pragma pack(push, 1)
/* FindFirst/Next data block - ALL DOS VERSIONS */
typedef struct {
  uint8_t drive_num;
  char srch_mask[11];
  uint8_t attr_mask;
  uint16_t sequence;
  uint16_t sector;
  uint8_t _reserved1[4];
} SRCHREC, far *SRCHREC_PTR;

/* DOS Directory entry for 'found' file - ALL DOS VERSIONS */
typedef struct {
  char name[11];
  uint8_t attr;
  uint8_t _reserved1[10];
  uint32_t time;
  uint16_t start_sector;
  long size;
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
  char file_name[128];
  char file_name_2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uint8_t _reserved3[81];
  char fcb_name[11];
  uint8_t _reserved4;
  char fcb_name_2[11];
  uint8_t _reserved5[11];
  uint8_t srch_attr;
  uint8_t open_mode;
  uint8_t _reserved6[48];
  uint8_t far *cdsptr;
  uint8_t _reserved7[72];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} SDAV3, far *SDA_PTR_V3;

/* Swappable DOS Area - DOS VERSION 4.xx */
typedef struct {
  uint8_t _reserved0[12];
  uint8_t far *current_dta;
  uint8_t _reserved1[32];
  uint8_t dd;
  uint8_t mm;
  uint16_t yy_1980;
  uint8_t _reserved2[106];
  char file_name[128];
  char file_name_2[128];
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
} SDAV4, far *SDA_PTR_V4;

/* DOS Current directory structure - DOS VERSION 3.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t _reserved1[10];
  uint16_t root_ofs;
} CDSV3, far *CDS_PTR_V3;

/* DOS Current directory structure - DOS VERSION 4.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t _reserved1[10];
  uint16_t root_ofs;
  uint8_t _reserved2[7];
} CDSV4, far *CDS_PTR_V4;

/* DOS List of lists structure - DOS VERSIONS 3.1 thru 4 */
/* We don't need much of it. */
typedef struct {
  uint8_t _reserved1[22];
  CDS_PTR_V3 cds_ptr;
  uint8_t _reserved2[7];
  uint8_t last_drive;
} LOLREC, far *LOLREC_PTR;

#pragma pack(pop)

#endif /* _DOSDATA_H */
