#ifndef _XMS_H
#define _XMS_H

#include "dosdata.h"

extern int xms_is_present(void);
extern uint16_t xms_kb_avail(void);
extern int xms_alloc_block(uint16_t block_size, uint16_t *handle_ptr);
extern int xms_free_block(uint16_t handle);
extern int xms_copy(uint16_t source, uint32_t source_offset, uint16_t dest, uint32_t dest_offset, uint32_t length);

#define xms_copy_to_real(xms_h, offset, len, buf) xms_copy(xms_h, offset, 0, (uint32_t) buf, len)
#define xms_copy_fm_real(xms_h, offset, len, buf) xms_copy(0, (uint32_t) buf, xms_h, offset, len)

#endif /* _XMS_H */
