#include "debug.h"

void dumpSFT(SFTREC_PTR sft)
{
  consolef("handle_count=%04x open_mode=%04x attr=%02x dev_info_word=%04x dev_drvr_ptr=%08lx start_sector/file_handle=%04x time=%04x date=%04x size=%08lx pos=%08lx rel_sector=%04x abs_sector=%04x dir_sector=%04x index=%02x fcb_name=\"%ls\"\n",
	   sft->handle_count, sft->open_mode, sft->attr, sft->dev_info_word, sft->dev_drvr_ptr,
#ifdef DIRECT_DRIVE
	   sft->start_sector,
#else
	   sft->file_handle,
#endif
	   sft->time, sft->date, sft->size, sft->pos,
#ifdef DIRECT_DRIVE
	   sft->rel_sector, sft->abs_sector,
#else
	   (uint16_t) (sft->last_pos & 0xffff), (uint16_t) (sft->last_pos >> 16),
#endif
	   sft->dir_sector, sft->index, sft->fcb_name);
  return;
}
