#include "commands.h"
#include "fujinet.h"
#include "sys_hdr.h"
#include "fujicom.h"
#include "print.h"
#include <string.h>
#include <dos.h>

#undef DEBUG

#define SECTOR_SIZE	512

extern void End_code(void);

struct BPB_struct fn_bpb_table[FN_MAX_DEV];
struct BPB_struct *fn_bpb_pointers[FN_MAX_DEV + 1]; // leave room for the NULL terminator

static uint8_t sector_buf[SECTOR_SIZE];
static cmdFrame_t cmd; // FIXME - make this shared with init.c?
static struct BPB_struct active_bpb[FN_MAX_DEV];

// time_t on FujiNet is 64 bits but that is too large to work
// with. Allocate twice as many 32b bit ints.
static int32_t mount_status[FN_MAX_DEV * 2];

uint16_t Media_check_cmd(SYSREQ far *req)
{
  int reply;
  int64_t old_status, new_status;


  if (req->unit >= FN_MAX_DEV) {
    consolef("Invalid Media Check unit: %i\n", req->unit);
    return ERROR_BIT | UNKNOWN_UNIT;
  }

  old_status = mount_status[req->unit * 2];

  cmd.device = DEVICEID_FUJINET;
  cmd.comnd = CMD_STATUS;
  cmd.aux1 = STATUS_MOUNT_TIME_L;
  cmd.aux2 = STATUS_MOUNT_TIME_H;

  reply = fujicom_command_read(&cmd, mount_status, sizeof(mount_status));
  if (reply != 'C')
    return ERROR_BIT | NOT_READY;

#if 0
  consolef("UNIT: %i\n", req->unit);
  consolef("MOUNT STATUS reply: 0x%04x\n", reply);
  dumpHex(mount_status, sizeof(mount_status));
#endif

  new_status = mount_status[req->unit * 2];

#if 0
  consolef("MEDIA CHECK: 0x%08lx == 0x%08lx\n", (uint32_t) old_status, (uint32_t) new_status);
#endif

  if (!new_status)
    req->req_type.media_check_req.return_info = 0;
  else if (old_status != new_status)
    req->req_type.media_check_req.return_info = -1;
  else
    req->req_type.media_check_req.return_info = 1;

  return OP_COMPLETE;
}

uint16_t Build_bpb_cmd(SYSREQ far *req)
{
  int reply;
  uint8_t far *buf;


  if (req->unit >= FN_MAX_DEV) {
    consolef("Invalid BPB unit: %i\n", req->unit);
    return ERROR_BIT | UNKNOWN_UNIT;
  }

  cmd.device = DEVICEID_DISK + req->unit;
  cmd.comnd = CMD_READ;
  cmd.aux1 = cmd.aux2 = 0;

  // DOS gave us a buffer to use?
  buf = req->req_type.build_bpb_req.buffer_ptr;
  reply = fujicom_command_read(&cmd, buf, sizeof(sector_buf));
  if (reply != 'C') {
    consolef("FujiNet read fail: %i\n", reply);
    return ERROR_BIT | READ_FAULT;
  }

  _fmemcpy(fn_bpb_pointers[req->unit], &buf[0x0b], sizeof(struct BPB_struct));

#if 0
  consolef("BPB for %i\n", req->unit);
  dumpHex((uint8_t far *) fn_bpb_pointers[req->unit], sizeof(struct BPB_struct));
#endif

  req->req_type.build_bpb_req.BPB_table = MK_FP(getCS(), fn_bpb_pointers[req->unit]);

  return OP_COMPLETE;
}

uint16_t Ioctl_input_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Input_cmd(SYSREQ far *req)
{
  int reply;
  uint16_t idx, sector = req->req_type.i_o_req.start_sector;
  uint8_t far *buf = req->req_type.i_o_req.buffer_ptr;


  if (req->unit >= FN_MAX_DEV) {
    consolef("Invalid Input unit: %i\n", req->unit);
    return ERROR_BIT | UNKNOWN_UNIT;
  }

#if 0
  dumpHex((uint8_t far *) req, req->length);
  consolef("SECTOR: 0x%x\n", sector);
#endif

  for (idx = 0; idx < req->req_type.i_o_req.count; idx++, sector++) {
    if (sector >= fn_bpb_table[req->unit].num_sectors) {
      consolef("FN Invalid sector read %i on %c:\n", sector, 'A' + req->unit);
      return ERROR_BIT | NOT_FOUND;
    }

    cmd.device = DEVICEID_DISK + req->unit;
    cmd.comnd = CMD_READ;
    cmd.aux1 = sector & 0xFF;
    cmd.aux2 = sector >> 8;

    reply = fujicom_command_read(&cmd, &buf[idx * SECTOR_SIZE], SECTOR_SIZE);
    if (reply != 'C')
      break;
  }
  if (!idx)
    return ERROR_BIT | GENERAL_FAIL;

  req->req_type.i_o_req.count = idx;
  return OP_COMPLETE;
}

uint16_t Input_no_wait_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Input_status_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Input_flush_cmd(SYSREQ far *req)
{
  return OP_COMPLETE;
}

uint16_t Output_cmd(SYSREQ far *req)
{
  req->req_type.i_o_req.count = 0;
  return OP_COMPLETE;
}

uint16_t Output_verify_cmd(SYSREQ far *req)
{
  req->req_type.i_o_req.count = 0;
  return OP_COMPLETE;
}

uint16_t Output_status_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Output_flush_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Ioctl_output_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Dev_open_cmd(SYSREQ far *req)
{
  return OP_COMPLETE;
}

uint16_t Dev_close_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Remove_media_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Ioctl_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Get_l_d_map_cmd(SYSREQ far *req)
{
  req->req_type.l_d_map_req.unit_code = 0;
  return OP_COMPLETE;
}

uint16_t Set_l_d_map_cmd(SYSREQ far *req)
{
  req->req_type.l_d_map_req.unit_code = 0;
  return OP_COMPLETE;
}

uint16_t Unknown_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}
