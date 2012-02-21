/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MISC_H_
#define XROAR_MISC_H_

/* For times when failure to alloc undermines basic operation, these wrappers
 * exit(1) on failure. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s1);

#endif  /* XROAR_MISC_H_ */
