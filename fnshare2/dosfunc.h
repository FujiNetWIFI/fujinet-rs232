#ifndef _DOSFUNC_H
#define _DOSFUNC_H

#include "types.h"

extern ulong dos_ftime(void);
extern void set_sft_owner(SFTREC_PTR sft);
extern int is_a_character_device(uint dos_ds);

#endif /* _DOSFUNC_H */
