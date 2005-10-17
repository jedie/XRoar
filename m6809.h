/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __M6809_H__
#define __M6809_H__

#include "machine.h"
#include "types.h"

extern int halt, nmi, firq, irq;

void m6809_init(void);
void m6809_reset(void);
void m6809_cycle(Cycle until);
void m6809_get_registers(uint8_t *regs);
void m6809_set_registers(uint8_t *regs);
void m6809_jump(uint_least16_t pc);

#endif  /* __M6809_H__ */
