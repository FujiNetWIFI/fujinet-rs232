#ifndef _RAMDRIVE_H
#define _RAMDRIVE_H

#include "dosdata.h"
#include "xms.h"

#define         SECTOR_SIZE             1024    // 1024b/sector allows for 64M of XMS

extern uint16_t xms_handle;
extern uint16_t last_sector;
extern uint8_t sector_buffer[];
extern uint16_t disk_size;
extern uint16_t total_sectors;
extern uint16_t free_sectors;

extern int get_dir_start_sector(char far *path, uint16_t far *abs_sector_ptr);
extern int find_next_entry(char far *mask, uint8_t attr_mask, char far *filename,
			   uint8_t far *attr_ptr, uint32_t far *file_time_ptr,
			   uint16_t far *start_sec_ptr, uint32_t far *file_size_ptr,
			   uint16_t far *dir_sector_ptr, uint16_t far *dir_entryno_ptr);
extern uint16_t set_next_sector(uint16_t abs_sector, uint16_t next_sector);
extern uint16_t next_free_sector(void);
extern int create_dir_entry(uint16_t far *dir_sector_ptr, uint8_t far *dir_entryno_ptr,
			    char far *filename, uint8_t file_attr, uint16_t start_sector,
			    uint32_t file_size, uint32_t file_time);
extern void read_data(uint32_t far *file_pos_ptr, uint16_t *len_ptr, uint8_t far *buf,
		      uint16_t start_sector, uint16_t far *last_rel_ptr, uint16_t far *last_abs_ptr);
extern void chop_file(uint32_t file_pos, uint16_t far *start_sec_ptr, uint16_t far *last_rel_ptr,
		      uint16_t far *last_abs_ptr);
extern void write_data(uint32_t far *file_pos_ptr, uint16_t *len_ptr, uint8_t far *buf,
		       uint16_t far *start_sec_ptr, uint16_t far *last_rel_ptr,
		       uint16_t far *last_abs_ptr);
extern void set_up_xms_disk(void);

#define get_sector(sec, buf)                                      \
        xms_copy_to_real(xms_handle, (uint32_t) SECTOR_SIZE * (sec), \
                SECTOR_SIZE, (uint8_t far *) (buf))

#define put_sector(sec, buf)            \
        xms_copy_fm_real(xms_handle, (uint32_t) SECTOR_SIZE * (sec), \
                SECTOR_SIZE, (uint8_t far *) (buf))

#define         FREE_SECTOR_CHAIN(sec)  \
        while ((sec) != 0xFFFF) (sec) = set_next_sector((sec), 0)

#endif /* _RAMDRIVE_H */
