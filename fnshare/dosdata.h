#ifndef _DOSDATA_H
#define _DOSDATA_H

#include <stdint.h>

#pragma pack(push, 1)
/* FindFirst/Next data block - ALL DOS VERSIONS */
typedef struct {
  uint8_t drive_no;
  char srch_mask[11];
  uint8_t attr_mask;
  uint16_t dir_entry_no;
  uint16_t dir_sector;
  uint8_t f1[4];
} SRCHREC, far *SRCHREC_PTR;

/* DOS Directory entry for 'found' file - ALL DOS VERSIONS */
typedef struct {
  char file_name[11];
  uint8_t file_attr;
  uint8_t f1[10];
  uint32_t file_time;
  uint16_t start_sector;
  long file_size;
} DIRREC, far *DIRREC_PTR;

/* Swappable DOS Area - DOS VERSION 3.xx */
typedef struct {
  uint8_t f0[12];
  uint8_t far *current_dta;
  uint8_t f1[30];
  uint8_t dd;
  uint8_t mm;
  uint16_t yy_1980;
  uint8_t f2[96];
  char file_name[128];
  char file_name_2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uint8_t f3[81];
  char fcb_name[11];
  uint8_t f4;
  char fcb_name_2[11];
  uint8_t f5[11];
  uint8_t srch_attr;
  uint8_t open_mode;
  uint8_t f6[48];
  uint8_t far *cdsptr;
  uint8_t f7[72];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} V3_SDA, far *V3_SDA_PTR;

/* Swappable DOS Area - DOS VERSION 4.xx */
typedef struct {
  uint8_t f0[12];
  uint8_t far *current_dta;
  uint8_t f1[32];
  uint8_t dd;
  uint8_t mm;
  uint16_t yy_1980;
  uint8_t f2[106];
  char file_name[128];
  char file_name_2[128];
  SRCHREC srchrec;
  DIRREC dirrec;
  uint8_t f3[88];
  char fcb_name[11];
  uint8_t f4;
  char fcb_name_2[11];
  uint8_t f5[11];
  uint8_t srch_attr;
  uint8_t open_mode;
  uint8_t f6[51];
  uint8_t far *cdsptr;
  uint8_t f7[87];
  uint16_t action_2E;
  uint16_t attr_2E;
  uint16_t mode_2E;
  uint8_t f8[29];
  SRCHREC rename_srchrec;
  DIRREC rename_dirrec;
} V4_SDA, far *V4_SDA_PTR;

/* DOS Current directory structure - DOS VERSION 3.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t f1[10];
  uint16_t root_ofs;
} V3_CDS, far *V3_CDS_PTR;

/* DOS Current directory structure - DOS VERSION 4.xx */
typedef struct {
  char current_path[67];
  uint16_t flags;
  uint8_t f1[10];
  uint16_t root_ofs;
  uint8_t f2[7];
} V4_CDS, far *V4_CDS_PTR;

/* DOS List of lists structure - DOS VERSIONS 3.1 thru 4 */
/* We don't need much of it. */
typedef struct {
  uint8_t f1[22];
  V3_CDS_PTR cds_ptr;
  uint8_t f2[7];
  uint8_t last_drive;
} LOLREC, far *LOLREC_PTR;

#pragma pack(pop)

#endif /* _DOSDATA_H */
