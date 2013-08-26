/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_PRINTER_H_
#define XROAR_PRINTER_H_

void printer_init(void);
void printer_reset(void);

void printer_open_file(const char *filename);
void printer_open_pipe(const char *command);
void printer_close(void);

void printer_flush(void);
void printer_strobe(void);

#endif  /* XROAR_PRINTER_H_ */
