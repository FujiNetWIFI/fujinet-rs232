#ifndef _DOSFUNC_H
#define _DOSFUNC_H

#include "dosdata.h"

extern uint32_t dos_ftime(void);
extern void set_sft_owner(SFTREC_PTR sft);
extern int is_a_character_device(uint16_t dos_ds);

#endif /* _DOSFUNC_H */
