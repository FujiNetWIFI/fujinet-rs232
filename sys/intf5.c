#include "print.h"
#include "commands.h"
#include "dispatch.h"
#include "pushpop.h"
#include <dos.h>
#include <fujicom.h>

static cmdFrame_t _cmdFrame;

#pragma data_seg("_CODE")

#define INTF5_NEAR
#ifdef INTF5_NEAR
void intf5(uint16_t dirdev, uint16_t command, uint16_t aux,
	   void far *ptr, uint16_t length)
#pragma aux intf5 __parm [ax] [cx] [dx] [es bx] [di] value [ax]
{
    int reply;

    _enable();

    consolef("INT F5\n");
    consolef("Dev: %04x Cmd: %04x Aux: %04x\n",
	     dirdev, command, aux);

    _cmdFrame.device = dirdev;
    _cmdFrame.comnd  = command;
    _cmdFrame.aux1   = aux & 0xff;
    _cmdFrame.aux2   = aux >> 8;

    switch (dirdev)
    {
    case FUJIINT_NONE:  // No Payload
        reply = fujicom_command(&_cmdFrame);
        break;
    case FUJIINT_READ:  // READ (Fujinet -> PC)
        reply = fujicom_command_read(&_cmdFrame, ptr, length);
        break;
    case FUJIINT_WRITE: // WRITE (PC -> FujiNet)
        reply = fujicom_command_write(&_cmdFrame, ptr, length);
        break;
    }

    consolef("INT F5 out\n");

    _asm {
        mov ax, reply;
	iret;
    }

    // IRET
}
#else /* !INTF5_NEAR */
static void __interrupt __far intf5(union INTPACK r)
{
    _enable();

    _cmdFrame.device = r.h.al;
    _cmdFrame.comnd  = r.h.cl;
    _cmdFrame.aux1   = r.h.dl;
    _cmdFrame.aux2   = r.h.dh;

    switch(r.h.ah)
    {
    case 0x00: // No Payload
        r.h.al = fujicom_command(&_cmdFrame);
        break;
    case 0x40: // READ (Fujinet -> PC)
        r.h.al = fujicom_command_read(&_cmdFrame,(uint8_t far *)MK_FP(r.w.es,r.w.bx),r.w.di);
        break;
    case 0x80: // WRITE (PC -> FujiNet)
        r.h.al = fujicom_command_write(&_cmdFrame,(uint8_t far *)MK_FP(r.w.es,r.w.bx),r.w.di);
        break;
    }

    // IRET
}
#endif /* INTF5_NEAR */

void setf5(void)
{
  _dos_setvect(0xF5, MK_FP(getCS(), intf5));
}
