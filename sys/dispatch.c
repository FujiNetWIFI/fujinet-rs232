#include "dispatch.h"
#include "commands.h"
#include "sys_hdr.h"
#include "pushpop.h"
#include "print.h" // For debugging only
#include <dos.h>
#include <stddef.h>

#undef DEBUG

#define STACK_SWAP
#ifdef STACK_SWAP
#define STACK_SIZE 1024
char our_stack[STACK_SIZE];     /* our internal stack */
uint16_t dos_ss;                    /* DOS's saved SS at entry */
uint16_t dos_sp;                    /* DOS's saved SP at entry */
uint16_t our_ss, our_sp;
#endif /* STACK_SWAP */

#define disable() _asm { cli }
#define enable() _asm { sti }

static SYSREQ __far *fpRequest = (SYSREQ __far *) 0;

typedef uint16_t(*driverFunction_t)(SYSREQ far *req);

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

void far Strategy(SYSREQ far *req)
#pragma aux Strategy __parm [__es __bx]
{
  fpRequest = req;
  return;
}

void far Interrupt(void)
#pragma aux Interrupt __parm []
{
  push_regs();

#ifdef STACK_SWAP
  // Save ss:sp and switch to our internal stack.

  our_sp = (FP_OFF(our_stack) + 15) >> 4;
  our_ss = FP_SEG(our_stack) + our_sp;
  our_sp = STACK_SIZE - 2 - (((our_sp - (FP_OFF(our_stack) >> 4)) << 4)
                             - (FP_OFF(our_stack) & 0xf));

  _asm {
    mov dos_ss, ss;
    mov dos_sp, sp;

    mov ax, our_ss;
    mov cx, our_sp;

    // activate new stack
    //cli;
    mov ss, ax;
    mov sp, cx;
    //sti;
  }
#endif /* STACK_SWAP */

  if (fpRequest->command > MAXCOMMAND
      || !(currentFunction = dispatchTable[fpRequest->command])) {
    fpRequest->status = ERROR_BIT | UNKNOWN_CMD;
  }
  else {
#ifdef DEBUG
    consolef("Command 0x%02x", fpRequest->command);
#endif
    fpRequest->status = currentFunction(fpRequest);
#ifdef DEBUG
    consolef(" result: 0x%04x\n", fpRequest->status);
#endif
  }

  fpRequest->status |= DONE_BIT;

#ifdef STACK_SWAP
  // Switch the stack back
  _asm {
    //cli;
    mov ss, dos_ss;
    mov sp, dos_sp;
    //sti;
  }
#endif /* STACK_SWAP */

  pop_regs();
  return;
}
