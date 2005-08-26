/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __XROAR_H__
#define __XROAR_H__

#include "types.h"

#define RESET_SOFT 0
#define RESET_HARD 1

extern Cycle current_cycle;

#ifdef TRACE
extern int trace;
# define IF_TRACE(s) if (trace) { s; }
#else
# define trace 0
# define IF_TRACE(s)
#endif

void xroar_getargs(int argc, char **argv);
void xroar_init(void);
void xroar_shutdown(void);
void xroar_reset(int hard);
void xroar_mainloop(void);

#endif  /* __XROAR_H__ */
