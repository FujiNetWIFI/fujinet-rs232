/**
 * low level FujiNet API
 */

#ifndef _FUJICOM_H
#define _FUJICOM_H

#undef FUJIF5_AS_FUNCTION

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
  uint16_t bw;
  uint8_t connected; /* meaning of this field is inconsistent */
  uint8_t error;
} fujiStatus;

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
  CMD_CHDIR                     = 0x2c,
  CMD_OPEN                      = 'O',
  CMD_CLOSE                     = 'C',
  CMD_READ                      = 'R',
  CMD_WRITE                     = 'W',
  CMD_STATUS                    = 'S',
  CMD_PARSE                     = 'P',
  CMD_QUERY                     = 'Q',
  CMD_APETIME_GETTIME           = 0x93,
  CMD_APETIME_SETTZ             = 0x99,
  CMD_APETIME_GETTZTIME         = 0x9A,
  CMD_READ_DEVICE_SLOTS         = 0xF2,
  CMD_JSON                      = 0xFC,
  CMD_USERNAME                  = 0xFD,
  CMD_PASSWORD                  = 0xFE,
};

enum {
  SLOT_READONLY                 = 1,
  SLOT_READWRITE                = 2,
};

enum {
  NETWORK_SUCCESS                               = 1,
  NETWORK_ERROR_WRITE_ONLY                      = 131,
  NETWORK_ERROR_INVALID_COMMAND                 = 132,
  NETWORK_ERROR_READ_ONLY                       = 135,
  NETWORK_ERROR_END_OF_FILE                     = 136,
  NETWORK_ERROR_GENERAL_TIMEOUT                 = 138,
  NETWORK_ERROR_GENERAL                         = 144,
  NETWORK_ERROR_NOT_IMPLEMENTED                 = 146,
  NETWORK_ERROR_FILE_EXISTS                     = 151,
  NETWORK_ERROR_NO_SPACE_ON_DEVICE              = 162,
  NETWORK_ERROR_INVALID_DEVICESPEC              = 165,
  NETWORK_ERROR_ACCESS_DENIED                   = 167,
  NETWORK_ERROR_FILE_NOT_FOUND                  = 170,
  NETWORK_ERROR_CONNECTION_REFUSED              = 200,
  NETWORK_ERROR_NETWORK_UNREACHABLE             = 201,
  NETWORK_ERROR_SOCKET_TIMEOUT                  = 202,
  NETWORK_ERROR_NETWORK_DOWN                    = 203,
  NETWORK_ERROR_CONNECTION_RESET                = 204,
  NETWORK_ERROR_CONNECTION_ALREADY_IN_PROGRESS  = 205,
  NETWORK_ERROR_ADDRESS_IN_USE                  = 206,
  NETWORK_ERROR_NOT_CONNECTED                   = 207,
  NETWORK_ERROR_SERVER_NOT_RUNNING              = 208,
  NETWORK_ERROR_NO_CONNECTION_WAITING           = 209,
  NETWORK_ERROR_SERVICE_NOT_AVAILABLE           = 210,
  NETWORK_ERROR_CONNECTION_ABORTED              = 211,
  NETWORK_ERROR_INVALID_USERNAME_OR_PASSWORD    = 212,
  NETWORK_ERROR_COULD_NOT_PARSE_JSON            = 213,
  NETWORK_ERROR_CLIENT_GENERAL                  = 214,
  NETWORK_ERROR_SERVER_GENERAL                  = 215,
  NETWORK_ERROR_COULD_NOT_ALLOCATE_BUFFERS      = 255,
};

enum {
  REPLY_ERROR           = 'E',
  REPLY_COMPLETE        = 'C',
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

#ifndef FUJIF5_AS_FUNCTION
extern int fujiF5w(uint16_t direction, uint16_t devcom,
                  uint16_t aux12, uint16_t aux34, void far *buffer, uint16_t length);
#pragma aux fujiF5w = \
  "int 0xf5" \
  parm [dx] [ax] [cx] [si] [es bx] [di] \
  modify [ax]
#define fujiF5(dx, dev, cmd, a12, a34, buf, len) \
  fujiF5w(dx, cmd << 8 | dev, a12, a34, buf, len)
#else
extern int fujiF5(uint8_t direction, uint8_t device, uint8_t command,
                  uint16_t aux12, uint16_t aux34, void far *buffer, uint16_t length);
#endif

#define fujiF5_none(d, c, a12, a34, b, l) fujiF5(FUJIINT_NONE, d, c, a12, a34, b, l)
#define fujiF5_read(d, c, a12, a34, b, l) fujiF5(FUJIINT_READ, d, c, a12, a34, b, l)
#define fujiF5_write(d, c, a12, a34, b, l) fujiF5(FUJIINT_WRITE, d, c, a12, a34, b, l)

#endif /* _FUJICOM_H */
