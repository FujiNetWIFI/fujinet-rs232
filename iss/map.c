/**
 * @brief Map display function
 */

#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include "map_data.h"

void map()
{
	_fmemcpy((void far *)MK_FP(0xB800,0),map_data,sizeof(map_data));
}
