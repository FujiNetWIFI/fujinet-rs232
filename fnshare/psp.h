#ifndef _PSP_H
#define _PSP_H

/* DOS Program Segment Prefix
 * https://en.wikipedia.org/wiki/Program_Segment_Prefix
 */

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
  uint16_t exit_intr;
  uint16_t top_segment;
  uint8_t reserved1;
  uint32_t dos_entry;
  uint8_t seg_count;
  uint32_t old_int22h;
  uint32_t old_int23h;
  uint32_t old_int24h;
  uint16_t parent_psp;
  uint8_t jft[20];
  uint16_t env_segment;
  uint32_t ss_sp;
  uint16_t jft_size;
  uint32_t jft_ptr;
  uint32_t prev_psp_ptr;
  uint32_t reserved2;
  uint16_t dos_version;
  uint8_t reserved3[14];
  uint8_t unix_call[3];
  uint16_t reserved4;
  uint8_t reserved5[7];
  uint8_t fcb1[16];
  uint8_t fcb2[20];
  uint8_t command_len;
  uint8_t command_tail[127];
} psp_t;
#pragma pack(pop)

#endif /* _PSP_H */
