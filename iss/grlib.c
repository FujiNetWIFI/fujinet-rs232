/**
 * @brief quick and dirty graphics lib
 */

#include <dos.h>

int gr_mode(int mode)
{
	union REGS r1,r2;

	if (mode<0)
	{
		r1.h.ah = 0x0f;
	}
	else
	{
		r1.h.ah = 0x00;
		r1.h.al = mode;
	}

	int86(0x10,&r1,&r2);

	r1.h.ah = 0x05;
	r1.h.al = 0x00;
	int86(0x10,&r1,&r1);

	return r2.h.al;
}

void gr_palette(unsigned char i, unsigned char c)
{
    union REGS r;

    r.h.ah = 0x10;
    r.h.al = 0x00;
    r.h.bl = i;
    r.h.bh = c;
    int86(0x10,&r,0);
}

void gr_color(char p, char c)
{
	union REGS r;

	r.h.ah = 0x0b;
	r.h.bh = 0x01;
	r.h.bl = p;

	/* int86(0x10,&r,0); */

	r.h.ah = 0x0b;
	r.h.bh = 0x00;
	r.h.bl = c;

	int86(0x10,&r,0);
}

void gr_pset(int x, int y, char c)
{
	union REGS r;

	r.h.ah = 0x0c;
	r.h.al = c;
	r.h.bh = 0;
	r.x.cx = x;
	r.x.dx = y;

	int86(0x10,&r,0);
}

void gr_text(int x, int y, char *s)
{
	union REGS r;

	/* Position cursor */
	r.h.ah = 0x02;
	r.h.bh = 0;
	r.h.dh = y;
	r.h.dl = x;
	int86(0x10,&r,0);

	/* Output characters */
	while (*s)
	{
		r.h.ah = 0x0e;
		r.h.al = *s;
		r.h.bl = 0x0f;
		int86(0x10,&r,0);

		s++;
	}
}
