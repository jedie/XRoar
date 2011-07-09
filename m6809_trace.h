/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_M6809_TRACE_H_
#define XROAR_M6809_TRACE_H_

void m6809_trace_reset(void);
void m6809_trace_byte(uint8_t byte, uint16_t pc);
void m6809_trace_irq(uint16_t vector);
void m6809_trace_print(uint8_t reg_cc, uint8_t reg_a,
		uint8_t reg_b, uint8_t reg_dp, uint16_t reg_x,
		uint16_t reg_y, uint16_t reg_u, uint16_t reg_s);

#endif  /* XROAR_M6809_TRACE_H_ */
