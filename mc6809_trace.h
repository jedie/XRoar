/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MC6809_TRACE_H_
#define XROAR_MC6809_TRACE_H_

#include "mc6809.h"

void mc6809_trace_reset(void);
void mc6809_trace_byte(uint8_t byte, uint16_t pc);
void mc6809_trace_irq(struct MC6809 *cpu, uint16_t vector);
void mc6809_trace_print(struct MC6809 *cpu);

#endif  /* XROAR_MC6809_TRACE_H_ */
