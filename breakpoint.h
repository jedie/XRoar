/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_BREAKPOINT_H_
#define XROAR_BREAKPOINT_H_

#include "m6809.h"

struct breakpoint;

/* add a new instruction breakpoint */
struct breakpoint *bp_add_instr(int addr, void (*handler)(M6809State *));
/* find any matching instruction breakpoint */
struct breakpoint *bp_find_instr(int addr, void (*handler)(M6809State *));

/* remove a referenced breakpoint - call free() manually */
void bp_remove(struct breakpoint *bp);
/* remove a referenced breakpoint and free() it */
void bp_delete(struct breakpoint *bp);

#endif  /* XROAR_BREAKPOINT_H_ */
