/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_BECKER_H_
#define XROAR_BECKER_H_

/* Support the so-called "becker port", an IP version of the usually-serial
 * DriveWire protocol. */

#include <inttypes.h>

_Bool becker_open(void);
void becker_close(void);
uint8_t becker_read_status(void);
uint8_t becker_read_data(void);
void becker_write_data(uint8_t D);

#endif  /* XROAR_BECKER_H_ */
