/**
 * low level FujiNet API
 */

#ifndef _FUJICOM_H
#define _FUJICOM_H

#include <stdint.h>

#define FUJINET_INT     0xF5
#define FUJIINT_NONE    0x00
#define FUJIINT_READ    0x40
#define FUJIINT_WRITE   0x80

#define FUJICOM_TIMEOUT  -1

// FIXME - get these constants and structs from
//         fujinet-firmware/lib/bus/rs232/rs232.h instead of
//         redefining them here

#pragma pack(push, 1)
typedef union {         /* Command Frame */
  struct {
    union {
      struct {
        uint8_t device; /* Destination Device */
        uint8_t comnd;  /* Command */
      };
      uint16_t devcom;
    };
    union {
      struct {
        uint8_t aux1;   /* Auxiliary Parameter 1 */
        uint8_t aux2;   /* Auxiliary Parameter 2 */
        uint8_t aux3;   /* Auxiliary Parameter 3 */
        uint8_t aux4;   /* Auxiliary Parameter 4 */
      };
      struct {
        uint16_t aux12;
        uint16_t aux34;
      };
      uint32_t aux;
    };
    uint8_t cksum;               /* 8-bit checksum */
  };
} cmdFrame_t;

typedef struct {
  unsigned char hostSlot;
  unsigned char mode;
  char file[36];
} deviceSlot_t;

#pragma pack(pop)

enum {
  DEVICEID_DISK                 = 0x31,
  DEVICEID_DISK_LAST            = 0x3F,
  DEVICEID_PRINTER              = 0x40,
  DEVICEID_PRINTER_LAST         = 0x43,
  DEVICEID_FN_VOICE             = 0x43,
  DEVICEID_APETIME              = 0x45,
  DEVICEID_RS232                = 0x50,
  DEVICEID_RS2323_LAST          = 0x53,
  DEVICEID_FUJINET              = 0x70,
  DEVICEID_FN_NETWORK           = 0x71,
  DEVICEID_FN_NETWORK_LAST      = 0x78,
  DEVICEID_MIDI                 = 0x99,
  DEVICEID_CPM                  = 0x5A,
};

enum {
  CMD_OPEN                      = 'O',
  CMD_CLOSE                     = 'C',
  CMD_READ                      = 'R',
  CMD_WRITE                     = 'W',
  CMD_STATUS                    = 'S',
  CMD_APETIME_GETTIME           = 0x93,
  CMD_APETIME_SETTZ             = 0x99,
  CMD_APETIME_GETTZTIME         = 0x9A,
  CMD_READ_DEVICE_SLOTS         = 0xF2,
  CMD_USERNAME                  = 0xFD,
  CMD_PASSWORD                  = 0xFE,
};

enum {
  MODE_READONLY                 = 1,
  MODE_READWRITE                = 2,
};

#define STATUS_MOUNT_TIME       0x01

/**
 * @brief start fujicom
 */
extern void fujicom_init(void);

/**
 * @brief calculate 8-bit checksum for cmdFrame_t.dcksum
 * @param buf Buffer to compute checksum against
 * @param len Length of aforementioned buffer
 */
extern uint8_t fujicom_cksum(void far *ptr, uint16_t len);

/**
 * @brief send FujiNet frame with no payload
 * @param cmdFrame Pointer to command frame
 * @return 'C'omplete, 'E'rror, or 'N'ak
 */
extern int fujicom_command(cmdFrame_t far *c);

/**
 * @brief send fujinet frame and read payload
 * @param cmdFrame pointer to command frame
 * @param buf Pointer to buffer to receive
 * @param len Expected buffer length
 */
extern int fujicom_command_read(cmdFrame_t far *c, void far *ptr, uint16_t len);

/**
 * @brief send fujinet frame and write payload
 * @param cmdFrame pointer to command frame
 * @param buf pointer to buffer to send.
 * @param len Length of buffer to send.
 */
extern int fujicom_command_write(cmdFrame_t far *c, void far *ptr, uint16_t len);

/**
 * @brief end fujicom
 */
void fujicom_done(void);

extern int fujiF5(uint8_t direction, uint8_t device, uint8_t command,
                  uint16_t aux12, uint16_t aux34, void far *buffer, uint16_t length);
#define fujiF5_none(d, c, a12, a34, b, l) fujiF5(FUJIINT_NONE, d, c, a12, a34, b, l)
#define fujiF5_read(d, c, a12, a34, b, l) fujiF5(FUJIINT_READ, d, c, a12, a34, b, l)
#define fujiF5_write(d, c, a12, a34, b, l) fujiF5(FUJIINT_WRITE, d, c, a12, a34, b, l)

#endif /* _FUJICOM_H */
