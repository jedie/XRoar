/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __M6809_TRACE_H__
#define __M6809_TRACE_H__

extern int m6809_trace_enabled;

#define m6809_trace_note(...) do { \
		if (m6809_trace_enabled) { \
			LOG_DEBUG(0, __VA_ARGS__); \
		} \
	} while (0)

void m6809_trace_helptext(void);
void m6809_trace_getargs(int argc, char **argv);

void m6809_trace_reset(void);
void m6809_trace_byte(unsigned int byte, unsigned int pc);
void m6809_trace_print(unsigned int reg_cc, unsigned int reg_a,
		unsigned int reg_b, unsigned int reg_dp, unsigned int reg_x,
		unsigned int reg_y, unsigned int reg_u, unsigned int reg_s);

#endif  /* __M6809_TRACE_H__ */
