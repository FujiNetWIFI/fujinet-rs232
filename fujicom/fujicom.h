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

typedef struct {               /* Command Frame */
  uint8_t device;              /* Destination Device */
  uint8_t comnd;               /* Command */
  uint8_t aux1;                /* Auxiliary Parameter 1 */
  uint8_t aux2;                /* Auxiliary Parameter 2 */
  uint8_t cksum;               /* 8-bit checksum */
} cmdFrame_t;

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
  CMD_USERNAME                  = 0xFD,
  CMD_PASSWORD                  = 0xFE,
};

#define STATUS_MOUNT_TIME_L     0x01
#define STATUS_MOUNT_TIME_H     0x00

/**
 * @brief start fujicom
 */
void fujicom_init(void);

/**
 * @brief calculate 8-bit checksum for cmdFrame_t.dcksum
 * @param buf Buffer to compute checksum against
 * @param len Length of aforementioned buffer
 */
uint8_t fujicom_cksum(void far *ptr, uint16_t len);

/**
 * @brief send FujiNet frame with no payload
 * @param cmdFrame Pointer to command frame
 * @return 'C'omplete, 'E'rror, or 'N'ak
 */
int fujicom_command(cmdFrame_t far *c);

/**
 * @brief send fujinet frame and read payload
 * @param cmdFrame pointer to command frame
 * @param buf Pointer to buffer to receive
 * @param len Expected buffer length
 */
int fujicom_command_read(cmdFrame_t far *c, void far *ptr, uint16_t len);

/**
 * @brief send fujinet frame and write payload
 * @param cmdFrame pointer to command frame
 * @param buf pointer to buffer to send.
 * @param len Length of buffer to send.
 */
int fujicom_command_write(cmdFrame_t far *c, void far *ptr, uint16_t len);

/**
 * @brief end fujicom
 */
void fujicom_done(void);

#endif /* _FUJICOM_H */
