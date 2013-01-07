/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_CRCLIST_H_
#define XROAR_CRCLIST_H_

#include <inttypes.h>

/* Parse an assignment string of the form "LIST=CRC[,CRC]..." */
void crclist_assign(const char *astring);

/* Attempt to find a CRC image.  If name starts with '@', search the named
 * list for the first accessible entry, otherwise search for a single entry. */
int crclist_match(const char *name, uint32_t crc);

/* Print a list of defined CRC lists to stdout */
void crclist_print(void);

#endif  /* XROAR_CRCLIST_H_ */
