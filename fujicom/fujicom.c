/**
 * #FUJINET Low Level Routines
 */

#undef DEBUG
#define INIT_INFO

#include "fujicom.h"
#include "com.h"
#include <dos.h>

#if defined(DEBUG) || defined(INIT_INFO)
#include "../sys/print.h" // debug
#endif

#include <env.h>
#include <stdlib.h>

#define TIMEOUT         100
#define TIMEOUT_SLOW	15 * 1000
#define MAX_RETRIES	5
#ifndef SERIAL_BPS
#define SERIAL_BPS      115200
#endif /* SERIAL_BPS */

PORT fn_port;
PORT far *port;
union REGS f5regs;
struct SREGS f5status;

void fujicom_init(void)
{
  int base, irq;
  long bps = SERIAL_BPS;
  int comp = 1;


  if (getenv("FUJI_PORT"))
    comp = atoi(getenv("FUJI_PORT"));
  if (getenv("FUJI_BPS"))
    bps = atol(getenv("FUJI_BPS"));

#if defined(DEBUG) || defined(INIT_INFO)
  consolef("Port: %i  BPS: %li\n", comp, (int32_t) bps);
#endif

  switch (comp) {
  default:
  case 1:
    base = COM1_UART;
    irq = COM1_INTERRUPT;
    break;
  case 2:
    base = COM2_UART;
    irq = COM2_INTERRUPT;
    break;
  case 3:
    base = COM3_UART;
    irq = COM3_INTERRUPT;
    break;
  case 4:
    base = COM4_UART;
    irq = COM4_INTERRUPT;
    break;
  }

  port = port_open(&fn_port, base, irq);
  port_set(port, bps, 'N', 8, 1);
  port_disable_interrupts(port);

  return;
}

uint8_t fujicom_cksum(void far *ptr, uint16_t len)
{
  uint16_t chk = 0;
  int i = 0;
  uint8_t far *buf = (uint8_t far *) ptr;


  for (i = 0; i < len; i++)
    chk = ((chk + buf[i]) >> 8) + ((chk + buf[i]) & 0xFF);
  return (uint8_t) chk;
}


/**
 * @brief Internal function, send command, get response.
 *
 * @param c ptr to command frame to send
 * @return 'A'ck, or 'N'ak.
 */
int _fujicom_send_command(cmdFrame_t far *cmd)
{
  uint8_t *cc = (uint8_t *) cmd;


  /* Calculate checksum and place in frame */
  cmd->cksum = fujicom_cksum(cc, sizeof(cmdFrame_t) - sizeof(cmd->cksum));

  /* Assert DTR to indicate start of command frame */
  port_set_dtr(port, 1);

  /* Write command frame */
  port_putbuf(port, cc, sizeof(cmdFrame_t));

  port_wait_for_tx_empty(port);
  /* Desert DTR to indicate end of command frame */
  port_set_dtr(port, 0);
  return port_getc_nobuf(port, TIMEOUT);
}

int fujicom_command(cmdFrame_t far *cmd)
{
  int reply;


  //port_disable_interrupts(port);
  _fujicom_send_command(cmd);
  reply = port_getc_nobuf(port, TIMEOUT);
  //port_enable_interrupts(port);
#if 0
#ifdef DEBUG
  consolef("FN command reply: 0x%04x\n", reply);
#endif
#endif

  return reply;
}

int fujicom_command_read(cmdFrame_t far *cmd, void far *ptr, uint16_t len)
{
  int reply;
  uint16_t rlen;
  int retries;
  uint8_t ck1, ck2;

  //port_disable_interrupts(port);

  for (retries = 0; retries < MAX_RETRIES; retries++) {
#ifdef DEBUG
    if (retries)
      consolef("FN read retry: %i\n", retries);
#endif

    // Flush any bytes left in the buffer
    port_wait_for_rx_empty(port);

    reply = _fujicom_send_command(cmd);
    if (reply == 'N')
      break;

    if (reply != 'A') {
#ifdef DEBUG
      consolef("FN send command bad: 0x%04x\n", reply);
#endif
      continue;
    }

    break;
  }

  if (retries == MAX_RETRIES)
    goto done;

  /* Get COMPLETE/ERROR */
  reply = port_getc_nobuf(port, TIMEOUT_SLOW);
  if (reply != 'C') {
#ifdef DEBUG
    consolef("FN complete fail: 0x%04x\n", reply);
#endif
    goto done;
  }

  /* Complete, get payload */
  rlen = port_getbuf(port, ptr, len, TIMEOUT);
  if (rlen != len) {
#ifdef DEBUG
    consolef("FN Read failed: Exp:%i Got:%i  Cmd: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
	     len, rlen,
	     cmd->device, cmd->comnd, cmd->aux1, cmd->aux2, cmd->cksum);
#endif
    reply = 'E';
    goto done;
  }

  /* Get Checksum byte, verify it. */
  ck1 = port_getc_nobuf(port, TIMEOUT_SLOW);
  ck2 = fujicom_cksum(ptr, len);

  if (ck1 != ck2) {
#ifdef DEBUG
    consolef("FN read checksum error: 0x%02x 0x%02x\n", ck1, ck2);
#endif
    reply = 'E';
  }

 done:
  //port_enable_interrupts(port);
#if 0
#ifdef DEBUG
  consolef("FN command read reply: 0x%04x\n", reply);
#endif
#endif
  return reply;
}

int fujicom_command_write(cmdFrame_t far *cmd, void far *ptr, uint16_t len)
{
  int reply;
  uint8_t ck;
  int retries;

  //port_disable_interrupts(port);

  for (retries = 0; retries < MAX_RETRIES; retries++) {
#ifdef DEBUG
    if (retries)
      consolef("FN write retry: %i\n", retries);
#endif

    // Flush any bytes left in the buffer
    port_wait_for_rx_empty(port);

    reply = _fujicom_send_command(cmd);
    if (reply == 'N')
      break;

    if (reply != 'A') {
#ifdef DEBUG
      consolef("FN write command bad: 0x%04x\n", reply);
#endif
      continue;
    }

    break;
  }

  if (retries == MAX_RETRIES)
    goto done;

  /* Write the payload */
  port_putbuf(port, ptr, len);

  /* Write the checksum */
  ck = fujicom_cksum(ptr, len);
  port_putc_nobuf(port, ck);

  /* Wait for ACK/NACK */
  reply = port_getc_nobuf(port, TIMEOUT);
  if (reply != 'A') {
#ifdef DEBUG
    consolef("FN write ack fail: 0x%04x\n", reply);
#endif
    goto done;
  }

  /* Wait for COMPLETE/ERROR */
  reply = port_getc_nobuf(port, TIMEOUT_SLOW);
  if (reply != 'C') {
#if 0
#ifdef DEBUG
    consolef("FN write complete fail: 0x%04x\n", reply);
#endif
#endif
  }

 done:
  //port_enable_interrupts(port)
#if 0
#ifdef DEBUG
  consolef("FN command write reply: 0x%04x\n", reply);
#endif
#endif
  return reply;
}

void fujicom_done(void)
{
  port_close(port);
  return;
}

#ifdef FUJIF5_AS_FUNCTION
int fujiF5(uint8_t direction, uint8_t device, uint8_t command,
	   uint16_t aux12, uint16_t aux34, void far *buffer, uint16_t length)
{
  f5regs.x.dx = direction;
  f5regs.h.al = device;
  f5regs.h.ah = command;
  f5regs.x.cx = aux12;
  f5regs.x.si = aux34;

  f5status.es  = FP_SEG(buffer);
  f5regs.x.bx = FP_OFF(buffer);
  f5regs.x.di = length;

  int86x(FUJINET_INT, &f5regs, &f5regs, &f5status);
  return f5regs.x.ax;
}
#endif
