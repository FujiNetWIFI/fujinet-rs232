/**
 * @brief   ISS tracker for MS-DOS systems
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license GPL v. 3, see LICENSE for details.
 */

#include <conio.h>
#include <time.h>
#include <dos.h>
#include "grlib.h"
#include "map.h"
#include "sat.h"

char lat[16], lon[16];
unsigned long ts;
int timeout=60000;

void fetch(char *lat, char *lon, unsigned long *ts);
void osd(char *lat, char *lon, unsigned long t);

int main()
{
	int oldmode = gr_mode(-1);

	gr_mode(4);
	gr_color(0,1);
	gr_palette(1,0x0A);

	while(1)
	{
		map();
		fetch(&lat,&lon,&ts);
		osd(&lat,&lon,ts);
		sat((char *)&lat,(char *)&lon);
		while (timeout--)
		{
			delay(1);
			if (kbhit())
			{
				switch (getch())
				{
					case 0x1B:
						goto bye;
					default:
						timeout=1;
						break;
				}
			}
		}
	}

bye:
	gr_mode(oldmode);
	return 0;
}
