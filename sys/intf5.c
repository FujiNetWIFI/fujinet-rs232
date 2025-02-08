#include <dos.h>
#include <i86.h>
#include <fujicom.h>
#include <stdint.h>
#include "print.h"
#include "commands.h"

static cmdFrame_t _cmdFrame;

#pragma data_seg("_CODE")

/*
 * AH		== direction
 * AL		== device
 * CL		== command
 * DL		== aux1
 * DH		== aux2
 * ES:BX	== far buffer pointer
 * DI		== buffer length
 */
int intf5(uint16_t dirdev, uint16_t command, uint16_t aux, void far *ptr, uint16_t length)
#pragma aux intf5 parm [ax] [cx] [dx] [es bx] [di] value [ax]
{
    int reply;

    _enable();

    consolef("INT F5 Dir: %04x Cmd: %04x Aux: %04x Ptr: %08lx Len: %04x\n",
	     dirdev, command, aux, (uint32_t) ptr, length);

    _cmdFrame.device = dirdev & 0xff;
    _cmdFrame.comnd  = command & 0xff;
    _cmdFrame.aux1   = aux & 0xff;
    _cmdFrame.aux2   = aux >> 8;

    switch (dirdev >> 8)
    {
    case FUJIINT_NONE: // No Payload
        reply = fujicom_command(&_cmdFrame);
        break;
    case FUJIINT_READ: // READ (Fujinet -> PC)
        reply = fujicom_command_read(&_cmdFrame, (uint8_t far *) ptr, length);
        break;
    case FUJIINT_WRITE: // WRITE (PC -> FujiNet)
        reply = fujicom_command_write(&_cmdFrame, (uint8_t far *) ptr, length);
        break;
    }

    consolef("INT F5 Reply: %04x\n", reply);
    return reply;
}

void setf5(void)
{
    extern void int_wrapper();
    void far *farptr;

    farptr = MK_FP(getCS(), int_wrapper);
    consolef("SET F5 %04x %08lx\n", int_wrapper, (uint32_t) farptr);
    _dos_setvect(0xF5, farptr);
}
