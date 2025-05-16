/**
 * Plot satellite at lat,lon
 */

#include <stdlib.h>
#include "latlon.h"
#include "grlib.h"

#define CENTER_X 4
#define CENTER_Y 4

/**
 * Satellite bitmap
 */
char satellite[8] =
{
	0x20,
	0x50,
	0xA4,
	0x58,
	0x1A,
	0x05,
	0x0A,
	0x04
};

void sat(char *lat_s, char *lon_s)
{
	int lat = atoi(lat_s);
	int lon = atoi(lon_s);
	int   x = longitude[lon+180]-CENTER_X;
	int   y = latitude[lat+90]-CENTER_Y;
	int   i = 0;
	int   j = 0;
	signed char b;

	for (i=0;i<8;i++)
	{
		b=satellite[i];
		for (j=0;j<8;j++)
		{
			if (b<0)
				gr_pset(x+j,y+i,3);
			else
				gr_pset(x+j,y+i,0);
			b <<= 1;
		}
	}
}
