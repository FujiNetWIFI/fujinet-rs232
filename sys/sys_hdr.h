/**
 * Dos Driver Header
 */

#ifndef _SYS_HDR_H
#define _SYS_HDR_H
#include <stdint.h>

/* Pack DOS structs to avoid padding; sizes aren’t word-aligned. Push
   current alignment parameters to restore later. */
#pragma pack(push, 1)

#define DOS_CMDS        25      /* # of DOS commands */
#define STK_SIZE        512     /* DOS Device Drive Stack */
#define DEVICES         1       /* # of block devices */
#define OP_COMPLETE     0x0000  /* Op complete return code */

#define CHAR_DD         0x8000  /* Character device driver */
#define IOCTL_SUP       0x4000  /* IOCTL supported */
#define NON_IBM         0x2000  /* Non-IBM format (Block) */
#define REMOVABLE       0x0800  /* Removable media (block */
#define GET_SET         0x0040  /* Get/Set Logical Device */
#define CLOCK_DD        0x0008  /* Current Clock Device */
#define NUL_DD          0x0004  /* Current NUL device */
#define STDOUT_DD       0x0002  /* Current stdout device */
#define STDIN_DD        0x0001  /* Current stdin device */
#define GEN_IOCTL       0x0001  /* Generic IOCTL if Block Device */

struct DEVICE_HEADER_struct {
  struct DEVICE_HEADER_struct far *next_hdr;
  uint16_t attribute;           /* Device Driver Attributes */
  uint16_t dev_strat;           /* ptr to strategy code */
  uint16_t dev_int;             /* ptr to interrupt code */
  uint8_t name_unit[8];         /* name/unit field */
};

#define ERROR_BIT       0x8000  /* Error Bit Mask */
#define ERROR_NUM       0x00FF  /* Error Number Mask */
#define DONE_BIT        0x0100  /* Device Operation Done */
#define BUSY_BIT        0x0200  /* Device Busy (not done) */

#define WRITE_PROTECT   0x00    /* Write Protect Violation */
#define UNKNOWN_UNIT    0x01    /* Unit Not Known by Driver */
#define NOT_READY       0x02    /* Device is not ready */
#define UNKNOWN_CMD     0x03    /* Unknown Device Command */
#define CRC_ERROR       0x04    /* Device CRC Error */
#define BAD_REQ_LEN     0x05    /* Bad Drive Req Struct Len */
#define SEEK_ERROR      0x06    /* Device Seek Error */
#define UNKNOWN_MEDIA   0x07    /* Unknown Media in Drive */
#define NOT_FOUND       0x08    /* Sector not Found */
#define OUT_OF_PAPER    0x09    /* Printer out of Paper */
#define WRITE_FAULT     0x0A    /* Device Write Fault */
#define READ_FAULT      0x0B    /* Device Read Fault */
#define GENERAL_FAIL    0x0C    /* General Device Failure */

/* Device Driver Command codes */

#define INIT            0       /* Initialize Device */
#define MEDIA_CHECK     1       /* Check for Correct Media */
#define BUILD_BPB       2       /* Build a BIOS Param Block (BPB) */
#define IOCTL_INPUT     3       /* IOCTL Input Requested */
#define INPUT           4       /* Device Read Operation */
#define INPUT_NO_WAIT   5       /* Input No Wait (CHAR ONLY) */
#define INPUT_STATUS    6       /* Input Status (CHAR ONLY) */
#define INPUT_FLUSH     7       /* Input Flush (CHAR ONLY) */
#define OUTPUT          8       /* Device Write Operation */
#define OUTPUT_VERIFY   9       /* Device Write with Verify */
#define OUTPUT_STATUS   10      /* Output Status (CHAR ONLY) */
#define OUTPUT_FLUSH    11      /* Output Flush (CHAR ONLY) */
#define IOCTL_OUTPUT    12      /* IOCTL Output Requested */
#define DEV_OPEN        13      /* Device Open Command */
#define DEV_CLOSE       14      /* Device Close Command */
#define REMOVE_MEDIA    15      /* Remove Media command (eject?) */
#define RESERVED_1      16      /* Reserved Command 1 */
#define RESERVED_2      17      /* Reserved Command 2 */
#define RESERVED_3      18      /* Reserved Command 3 */
#define IOCTL           19      /* IOCTL */
#define RESERVED_4      20      /* Reserved Command 4 */
#define RESERVED_5      21      /* Reserved Command 5 */
#define RESERVED_6      22      /* Reserved Command 6 */
#define GET_L_D_MAP     23      /* Get Logical Drive Map */
#define SET_L_D_MAP     24      /* Set Logical Drive Map */
#define MAXCOMMAND      SET_L_D_MAP

typedef struct {
  uint16_t bps;                 /* Bytes per Sector */
  uint8_t spau;                 /* Sectors/allocation unit */
  uint16_t rs;                  /* Reserved Sectors */
  uint8_t num_FATs;             /* # of FATs */
  uint16_t root_entries;        /* # of Root Dir Entries */
  uint16_t num_sectors;         /* # of sectors */
  uint8_t media_descriptor;     /* Media Descriptor */
  uint16_t spfat;               /* # of sectors per FAT */
  uint16_t spt;                 /* # of sectors per track */
  uint16_t heads;               /* # of heads */
  uint32_t hidden;              /* # of hidden sectors */
  uint32_t num_sectors_32;      /* 32-bit # of sectors */
} DOS_BPB;

typedef struct {
  uint8_t num_units;
  uint8_t far *end_ptr;         /* end addr of driver */
  uint8_t far *bpb_ptr;         /* pointer to init args */

  /* Set to BPB array on exit */
  uint8_t drive_num;            /* driver # */
  uint16_t config_err;          /* config.sys error flag */
} SYSREQ_INIT;

typedef struct {
  uint8_t media_descr;          /* Media Descriptor from MS-DOS */
  uint8_t return_info;          /* Return Information */
  uint8_t far *return_ptr;      /* Pointer to prev VolID */
} SYSREQ_MEDIA;

typedef struct {
  uint8_t media_descr;          /* Media Descriptor from MS-DOS */
  uint8_t far *buffer_ptr;      /* Pointer to buffer */
  DOS_BPB far *table;           /* Pointer to BPB Table */
} SYSREQ_BPB;

typedef struct {
  uint8_t media_descr;          /* Media Desc from MS-DOS */
  uint8_t far *buffer_ptr;      /* Pointer To Buffer */
  uint16_t count;               /* Byte/Sector Count */
  uint16_t start_sector;        /* Starting sector number */
  uint8_t far *vol_id_ptr;      /* ptr to Volume ID */
  uint32_t start_sector_32;     /* 32-bit starting sector */
} SYSREQ_IO;

typedef struct {
  uint8_t byte_read;            /* byte read from device */
} SYSREQ_INPUT;

typedef struct {
  uint8_t major_func;           /* Function (Major) */
  uint8_t minor_func;           /* Function (Minor) */
  uint16_t si_reg;              /* Contents of SI Register */
  uint16_t di_reg;              /* Contents of DI Register */

  /* ptr to Request Packet */
  uint8_t far *ioctl_req_ptr;
} SYSREQ_IOCTL;

typedef struct {
  uint8_t unit_code;            /* Input - Unit Code */

  /* output - last device used */
  uint8_t cmd_code;             /* Command Code */
  uint16_t status;              /* Status Word */
  uint32_t reserved;            /* DOS Reserved */
} SYSREQ_LDMAP;

typedef struct {
  uint8_t length;               /* Length in bytes of Request */
  uint8_t unit;                 /* Minor Device Unit Number */
  uint8_t command;              /* Device Command Code */
  uint16_t status;              /* Device Status Word */
  uint8_t reserved[8];          /* Reserved for MS-DOS */
  union {
    SYSREQ_INIT init;
    SYSREQ_MEDIA media;
    SYSREQ_BPB bpb;
    SYSREQ_IO io;
    SYSREQ_INPUT input;
    SYSREQ_LDMAP ldmap;
  };
} SYSREQ;

/* Restore pushed struct alignment parameters */
#pragma pack(pop)

#endif /* _SYS_HDR_H */
