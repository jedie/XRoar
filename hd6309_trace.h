/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_HD6309_TRACE_H_
#define XROAR_HD6309_TRACE_H_

#include "hd6309.h"

void hd6309_trace_reset(void);
void hd6309_trace_byte(uint8_t byte, uint16_t pc);
void hd6309_trace_irq(struct MC6809 *cpu, uint16_t vector);
void hd6309_trace_print(struct MC6809 *cpu);

#endif  /* XROAR_HD6309_TRACE_H_ */
