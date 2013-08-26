/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_ROMLIST_H_
#define XROAR_ROMLIST_H_

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]..." */
void romlist_assign(const char *astring);

/* Attempt to find a ROM image.  If name starts with '@', search the named
 * list for the first accessible entry, otherwise search for a single entry. */
char *romlist_find(const char *name);

/* Print a list of defined ROM lists to stdout */
void romlist_print(void);

#endif  /* XROAR_ROMLIST_H_ */
