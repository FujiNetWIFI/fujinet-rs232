/**
 * OSD
 */

#include <stdio.h>
#include <time.h>
#include "grlib.h"
#include "ftime.h"

void osd(char *lat, char *lon, unsigned long t)
{
	char tmp[41];
        char buf[26];
        struct tm tod;
        
	gr_text(10,20,"CURRENT ISS POSITION");

        _localtime(&t,&tod);
        sprintf(tmp,"          %s",_asctime(&tod,buf));
        
	gr_text(0,21,tmp);
	sprintf(tmp,"           LATITUDE: %10s",lat);
	gr_text(0,22,tmp);
	sprintf(tmp,"          LONGITUDE: %10s",lon);
	gr_text(0,23,tmp);
}
