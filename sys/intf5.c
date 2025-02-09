#include <dos.h>
#include <i86.h>
#include <fujicom.h>
#include <stdint.h>
#include "print.h"
#include "commands.h"

static cmdFrame_t _cmdFrame;

#pragma data_seg("_CODE")

/*
 * DL		== direction
 * AL		== device
 * AH		== command
 * CL		== aux1
 * CH		== aux2
 * SI           == aux34
 * ES:BX	== far buffer pointer
 * DI		== buffer length
 */
int intf5(uint16_t direction, uint16_t devcom, uint16_t aux12, uint16_t aux34,
	  void far *ptr, uint16_t length)
#pragma aux intf5 parm [dx] [ax] [cx] [si] [es bx] [di] value [ax]
{
    int reply;

    _enable();

    _cmdFrame.devcom = devcom;
    _cmdFrame.aux12 = aux12;
    _cmdFrame.aux34 = aux34;

    switch (direction)
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

    return reply;
}

void setf5(void)
{
    extern void intf5_vect();

    _dos_setvect(0xF5, MK_FP(getCS(), intf5_vect));
}
