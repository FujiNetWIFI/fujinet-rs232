#ifndef _XMS_H
#define _XMS_H

#include "types.h"

extern int xms_is_present(void);
extern uint xms_kb_avail(void);
extern int xms_alloc_block(uint block_size, uint *handle_ptr);
extern int xms_free_block(uint handle);
extern int xms_copy(uint source, ulong source_offset, uint dest, ulong dest_offset, ulong length);

#define xms_copy_to_real(xms_h, offset, len, buf) xms_copy(xms_h, offset, 0, (ulong) buf, len)
#define xms_copy_fm_real(xms_h, offset, len, buf) xms_copy(0, (ulong) buf, xms_h, offset, len)

#endif /* _XMS_H */
