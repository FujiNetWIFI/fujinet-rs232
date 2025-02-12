#include "commands.h"
#include "fujinet.h"
#include "sys_hdr.h"
#include "fujicom.h"
#include "print.h"
#include <string.h>
#include <dos.h>

#undef DEBUG

#define SECTOR_SIZE     512

extern void End_code(void);

DOS_BPB fn_bpb_table[FN_MAX_DEV];
DOS_BPB *fn_bpb_pointers[FN_MAX_DEV + 1]; // leave room for the NULL terminator

static cmdFrame_t cmd; // FIXME - make this shared with init.c?

// time_t on FujiNet is 64 bits but that is too large to work
// with. Allocate twice as many 32b bit ints.
static int32_t mount_status[FN_MAX_DEV * 2];

static deviceSlot_t disk_slots[FN_MAX_DEV];

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
  cmd.aux = STATUS_MOUNT_TIME;
  reply = fujicom_command_read(&cmd, mount_status, sizeof(mount_status));
  if (reply != 'C')
    return ERROR_BIT | NOT_READY;

  // Get read/write state while we're at it
  cmd.device = DEVICEID_FUJINET;
  cmd.comnd = CMD_READ_DEVICE_SLOTS;
  cmd.aux = 0;
  reply = fujicom_command_read(&cmd, disk_slots, sizeof(disk_slots));
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
    req->media.return_info = 0;
  else if (old_status != new_status)
    req->media.return_info = -1;
  else
    req->media.return_info = 1;

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
  cmd.aux = 0;

  // DOS gave us a buffer to use
  buf = req->bpb.buffer_ptr;
  reply = fujicom_command_read(&cmd, buf, SECTOR_SIZE);
  if (reply != 'C') {
    consolef("FujiNet read fail: %i\n", reply);
    return ERROR_BIT | READ_FAULT;
  }

  _fmemcpy(fn_bpb_pointers[req->unit], &buf[0x0b], sizeof(DOS_BPB));

#if 0
  consolef("BPB for %i\n", req->unit);
  dumpHex((uint8_t far *) fn_bpb_pointers[req->unit], sizeof(DOS_BPB));
#endif

  req->bpb.table = MK_FP(getCS(), fn_bpb_pointers[req->unit]);

  return OP_COMPLETE;
}

uint16_t Ioctl_input_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}

uint16_t Input_cmd(SYSREQ far *req)
{
  int reply;
  uint16_t idx;
  uint32_t sector, sector_max;
  uint8_t far *buf = req->io.buffer_ptr;


  if (req->unit >= FN_MAX_DEV) {
    consolef("Invalid Input unit: %i\n", req->unit);
    return ERROR_BIT | UNKNOWN_UNIT;
  }

#if 0
  dumpHex((uint8_t far *) req, req->length);
  consolef("SECTOR: %i 0x%04x 0x%08lx\n", req->length,
           req->io.start_sector, (uint32_t) req->io.start_sector_32);
#endif

  if (req->length > 22)
    sector = req->io.start_sector_32;
  else
    sector = req->io.start_sector;

  if (fn_bpb_table[req->unit].num_sectors)
    sector_max = fn_bpb_table[req->unit].num_sectors;
  else
    sector_max = fn_bpb_table[req->unit].num_sectors_32;

#if 0
  consolef("SECTOR: %i 0x%08lx %i\n", req->length, sector, req->io.count);
#endif

  for (idx = 0; idx < req->io.count; idx++, sector++) {
    if (sector >= sector_max) {
      consolef("FN Invalid sector read %li on %i\n", sector, req->unit);
      return ERROR_BIT | NOT_FOUND;
    }

    cmd.device = DEVICEID_DISK + req->unit;
    cmd.comnd = CMD_READ;
    cmd.aux = sector;

    reply = fujicom_command_read(&cmd, &buf[idx * SECTOR_SIZE], SECTOR_SIZE);
    if (reply != 'C')
      break;
  }
  if (!idx)
    return ERROR_BIT | GENERAL_FAIL;

  req->io.count = idx;
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
  consolef("FN INPUT FLUSH\n");
  return ERROR_BIT | GENERAL_FAIL;
}

uint16_t Output_cmd(SYSREQ far *req)
{
  int reply;
  uint16_t idx;
  uint32_t sector, sector_max;
  uint8_t far *buf = req->io.buffer_ptr;


  if (req->unit >= FN_MAX_DEV) {
    consolef("Invalid Output unit: %i\n", req->unit);
    return ERROR_BIT | UNKNOWN_UNIT;
  }

  if (disk_slots[req->unit].mode != MODE_READWRITE)
    return ERROR_BIT | WRITE_PROTECT;

  if (req->length > 22)
    sector = req->io.start_sector_32;
  else
    sector = req->io.start_sector;

  if (fn_bpb_table[req->unit].num_sectors)
    sector_max = fn_bpb_table[req->unit].num_sectors;
  else
    sector_max = fn_bpb_table[req->unit].num_sectors_32;

#if 0
  consolef("WRITE SECTOR: %i 0x%08lx %i\n", req->length, sector, req->io.count);
#endif

  for (idx = 0; idx < req->io.count; idx++, sector++) {
    if (sector >= sector_max) {
      consolef("FN Invalid sector write %i on %i:\n", sector, req->unit);
      return ERROR_BIT | NOT_FOUND;
    }

    cmd.device = DEVICEID_DISK + req->unit;
    cmd.comnd = CMD_WRITE;
    cmd.aux = sector;

    reply = fujicom_command_write(&cmd, &buf[idx * SECTOR_SIZE], SECTOR_SIZE);
    if (reply != 'C')
      break;
  }
  if (!idx)
    return ERROR_BIT | GENERAL_FAIL;

  req->io.count = idx;
  return OP_COMPLETE;
}

uint16_t Output_verify_cmd(SYSREQ far *req)
{
  consolef("FN OUTPUT VERIFY\n");
  req->io.count = 0;
  return ERROR_BIT | GENERAL_FAIL;
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
  consolef("FN DEV OPEN\n");
  return ERROR_BIT | GENERAL_FAIL;
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
  consolef("FN GET LD MAP\n");
  req->ldmap.unit_code = 0;
  return ERROR_BIT | GENERAL_FAIL;
}

uint16_t Set_l_d_map_cmd(SYSREQ far *req)
{
  consolef("FN SET LD MAP\n");
  req->ldmap.unit_code = 0;
  return ERROR_BIT | GENERAL_FAIL;
}

uint16_t Unknown_cmd(SYSREQ far *req)
{
  return UNKNOWN_CMD;
}
