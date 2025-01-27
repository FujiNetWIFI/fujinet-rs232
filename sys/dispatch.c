#include "commands.h"
#include "sys_hdr.h"
#include "pushpop.h"
#include <dos.h>
#include <stddef.h>

#define disable() _asm { cli }
#define enable() _asm { sti }

SYSREQ __far *fpRequest = (SYSREQ __far *) 0;
extern uint16_t local_stk[STK_SIZE];

extern struct REQ_struct far *r_ptr;

typedef uint16_t(*driverFunction_t) (SYSREQ far *r_ptr);

static driverFunction_t currentFunction;
static driverFunction_t dispatchTable[] = {
  Init_cmd,
  Media_check_cmd,
  Build_bpb_cmd,
  Ioctl_input_cmd,
  Input_cmd,
  Input_no_wait_cmd,
  Input_status_cmd,
  Input_flush_cmd,
  Output_cmd,
  Output_verify_cmd,
  Output_status_cmd,
  Output_flush_cmd,
  Ioctl_output_cmd,
  Dev_open_cmd,
  Dev_close_cmd,
  Remove_media_cmd,
  Unknown_cmd,
  Unknown_cmd,
  Unknown_cmd,
  Ioctl_cmd,
  Unknown_cmd,
  Unknown_cmd,
  Unknown_cmd,
  Get_l_d_map_cmd,
  Set_l_d_map_cmd
};

void far Strategy(SYSREQ far *r_ptr)
#pragma aux Strategy __parm [__es __bx]
{
  fpRequest = r_ptr;
  return;
}

void far Interrupt(void)
#pragma aux Interrupt __parm []
{
  push_regs();

  if (fpRequest->command > MAXCOMMAND
      || !(currentFunction = dispatchTable[fpRequest->command])) {
    fpRequest->status = DONE_BIT | ERROR_BIT | UNKNOWN_CMD;
  }
  else {
    fpRequest->status = currentFunction(NULL);
  }

  pop_regs();
  return;
}
