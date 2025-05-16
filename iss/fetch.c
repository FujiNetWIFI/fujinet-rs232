/**
 * Fetch routine
 */

#include "fujicom.h"
#include <string.h>
#include <dos.h>

const char url[256]="N:HTTP://api.open-notify.org/iss-now.json";
const char query_lon[256]="N:/iss_position/longitude";
const char query_lat[256]="N:/iss_position/latitude";
const char query_ts[256]="N:/timestamp";
union REGS r;
struct SREGS sr;

struct _s
{
    unsigned short bw;
    unsigned char connected;
    unsigned char error;
} s;

void fetch_query(const char *q, char *result)
{
    /* Query FujiNet for JSON path */
    r.h.dl = 0x80;
    r.h.ah = 'Q';
    r.h.al = 0x71;
    r.x.cx = 0x0000;
    r.x.si = 0x0000;
    sr.es = FP_SEG(q);
    r.x.bx = FP_OFF(q);
    r.x.di = 256;
    int86x(0xF5,&r,&r,&sr);

    /* GET # of bytes waiting for result */
    r.h.dl = 0x40;
    r.h.ah = 'S';
    r.h.al = 0x71;
    r.x.cx = 0x0000;
    r.x.si = 0x0000;
    sr.es = FP_SEG(&s);
    r.x.bx = FP_OFF(&s);
    r.x.di = sizeof(s);
    int86x(0xF5,&r,&r,&sr);

    /* Read Result into buffer */
    r.h.dl = 0x40;
    r.h.ah = 'R';
    r.h.al = 0x71;
    r.x.cx = s.bw;
    r.x.si = 0x0000;
    sr.es = FP_SEG(result);
    r.x.bx = FP_OFF(result);
    r.x.di = s.bw;
    int86x(0xF5,&r,&r,&sr);
}

void fetch(char *lat, char *lon, unsigned long *ts)
{
    char ts_s[16];

    memset(ts_s,0,sizeof(ts_s));

    /* /\* open *\/ */
    r.h.dl = 0x80;
    r.h.ah = 'O';
    r.h.al = 0x71;
    r.h.cl = 0x0c;
    r.h.ch = 0x00;
    r.x.si = 0x0000;
    sr.es = FP_SEG(url);
    r.x.bx = FP_OFF(url);
    r.x.di = 256;
    int86x(0xF5,&r,&r,&sr);

    /* /\* Set channel mode to JSON *\/ */
    r.h.dl = 0x00;
    r.h.ah = 0xFC;
    r.h.al = 0x71;
    r.h.cl = 0x00;
    r.h.ch = 0x01;
    r.x.si = 0x0000;
    int86(0xF5,&r,&r);

    /* /\* Parse incoming JSON *\/ */
    r.h.dl = 0x00;
    r.h.ah = 'P';
    r.h.al = 0x71;
    r.x.cx = 0x0000;
    r.x.si = 0x0000;
    int86(0xF5,&r,&r);

    /* Query and fetch data */
    fetch_query(query_ts,ts_s);
    fetch_query(query_lon,lon);
    fetch_query(query_lat,lat);
}
