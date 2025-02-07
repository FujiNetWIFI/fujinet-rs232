#include <dos.h>
#include <i86.h>
#include <fujicom.h>
#include "print.h"

static cmdFrame_t _cmdFrame;

#pragma data_seg("_CODE")

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

void setf5(void)
{
#ifdef INTF5_RELOC_IS_FIXED
    _dos_setvect(0xF5, intf5);
#endif
}
