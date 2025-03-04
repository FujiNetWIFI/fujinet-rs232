#ifndef _RAMDRIVE_H
#define _RAMDRIVE_H

#include "types.h"
#include "xms.h"

#define         SECTOR_SIZE             1024    // 1024b/sector allows for 64M of XMS

extern uint xms_handle;
extern uint last_sector;
extern uchar sector_buffer[];
extern uint disk_size;
extern uint total_sectors;
extern uint free_sectors;

extern int get_dir_start_sector(char far *path, uint far *abs_sector_ptr);
extern int find_next_entry(char far *mask, uchar attr_mask, char far *filename,
			   uchar far *attr_ptr, ulong far *file_time_ptr,
			   uint far *start_sec_ptr, long far *file_size_ptr,
			   uint far *dir_sector_ptr, uint far *dir_entryno_ptr);
extern uint set_next_sector(uint abs_sector, uint next_sector);
extern uint next_free_sector(void);
extern int create_dir_entry(uint far *dir_sector_ptr, uchar far *dir_entryno_ptr,
			    char far *filename, uchar file_attr, uint start_sector,
			    long file_size, ulong file_time);
extern void read_data(long far *file_pos_ptr, uint *len_ptr, uchar far *buf,
		      uint start_sector, uint far *last_rel_ptr, uint far *last_abs_ptr);
extern void chop_file(long file_pos, uint far *start_sec_ptr, uint far *last_rel_ptr,
		      uint far *last_abs_ptr);
extern void write_data(long far *file_pos_ptr, uint *len_ptr, uchar far *buf,
		       uint far *start_sec_ptr, uint far *last_rel_ptr,
		       uint far *last_abs_ptr);
extern void set_up_xms_disk(void);

#define get_sector(sec, buf)                                      \
        xms_copy_to_real(xms_handle, (ulong) SECTOR_SIZE * (sec), \
                SECTOR_SIZE, (uchar far *) (buf))

#define put_sector(sec, buf)            \
        xms_copy_fm_real(xms_handle, (ulong) SECTOR_SIZE * (sec), \
                SECTOR_SIZE, (uchar far *) (buf))

#define         FREE_SECTOR_CHAIN(sec)  \
        while ((sec) != 0xFFFF) (sec) = set_next_sector((sec), 0)

#endif /* _RAMDRIVE_H */
