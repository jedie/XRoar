/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_PRINTER_H_
#define XROAR_PRINTER_H_

void printer_init(void);
void printer_reset(void);

/* Printer ACK delegate is passed its data pointer and ACK state as _Bool */
typedef struct {
	void (*delegate)(void *, _Bool);
	void *dptr;
} printer_line_delegate;

extern printer_line_delegate printer_signal_ack;

void printer_open_file(const char *filename);
void printer_open_pipe(const char *command);
void printer_close(void);

void printer_flush(void);
void printer_strobe(_Bool strobe, int data);
_Bool printer_busy(void);

#endif  /* XROAR_PRINTER_H_ */
