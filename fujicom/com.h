/*
 *  Listing 2  -  COM.H  -  "RS-232 Interrupts The C Way"
 *
 *  Copyright (C) Mark R. Nelson 1990
 *
 * This file contains the structure definitions, constants,
 * and function prototypes needed to use the RS-232
 * interface code supplied in COM.C.  It should be included
 * by any routine using COM.C procedures.
 */

#ifndef _COM_H
#define _COM_H

#include <stdint.h>

/*
 * This structure defines the 256 byte buffer used for
 * I/O buffers by the COM routines.  By using a buffer
 * size of 256 bytes, updating the indices is simplified.
 */
typedef struct {
  char buffer[256];
  uint8_t write_index;
  uint8_t read_index;
} BUFFER;

/*
 * This structure defines a COM port.  It is initialized
 * when the port is opened with port_open().
 */
typedef struct {
  void (interrupt far *old_vector) ();
  int uart_base;
  int irq_mask;
  int interrupt_number;
  BUFFER in;
  BUFFER out;
} PORT;

/*
 * the ifdef M_186 is checking for Microsoft C/QuickC.
 * Borland and Microsoft differ slightly on the names of
 * some of the DOS specific procedure names, and the
 * fixing up is done here.
 */
#ifdef M_I86
#define inportb inp
#define outportb outp
#define getvect _dos_getvect
#define setvect _dos_setvect
#define enable _enable
#endif /*  */
/*
 * The fully qualified function prototypes.  All of these
 * routines actually reside in COM.C
 */
extern PORT far *port_open(PORT far *port, int address, int interrupt_number);
extern void port_set(PORT far *port, long speed, char parity, int data, int stopbits);
extern void port_close(PORT far *port);
extern int port_putc(PORT far *port, uint8_t c);
extern int port_available(PORT far *port);
extern int port_getc(PORT far *port);
extern int port_getc_sync(PORT far *port, uint16_t timeout);
extern void port_set_dtr(PORT far *port, uint8_t t);
extern void port_put(PORT far *port, uint8_t far *buf, uint16_t len);
extern void port_disable_interrupts(PORT far *port);
extern void port_enable_interrupts(PORT far *port);
extern uint16_t port_putbuf(PORT far *port, uint8_t far *buf, uint16_t len);
extern uint16_t port_getbuf(PORT far *port, uint8_t far *buf, uint16_t len, uint16_t timeout);
extern void port_putc_nobuf(PORT far *port, uint8_t c);
extern int port_getc_nobuf(PORT far *port, uint16_t timeout);
extern void port_wait_for_tx_empty(PORT far *part);
extern void port_wait_for_rx_empty(PORT far *port);
extern int port_identify_uart(PORT far *port);

/*
 * These are the standard UART addresses and interrupt
 * numbers for the four IBM compatible COM ports.
 */
#define COM1_UART         0x3f8
#define COM1_INTERRUPT    12
#define COM2_UART         0x2f8
#define COM2_INTERRUPT    11
#define COM3_UART         0x3e8
#define COM3_INTERRUPT    12
#define COM4_UART         0x2e8
#define COM4_INTERRUPT    11

enum {
  UART_8250 = 0,
  UART_16450,
  UART_16550,
  UART_16550A,
};

#endif /* _COM_H */
