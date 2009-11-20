/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __M6809_TRACE_H__
#define __M6809_TRACE_H__

void m6809_trace_reset(void);
void m6809_trace_byte(unsigned int byte, unsigned int pc);
void m6809_trace_irq(unsigned int vector);
void m6809_trace_print(unsigned int reg_cc, unsigned int reg_a,
		unsigned int reg_b, unsigned int reg_dp, unsigned int reg_x,
		unsigned int reg_y, unsigned int reg_u, unsigned int reg_s);

#endif  /* __M6809_TRACE_H__ */
