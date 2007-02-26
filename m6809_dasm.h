/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __M6809_DASM_H__
#define __M6809_DASM_H__

#ifdef TRACE
void m6809_dasm_reset(void);
void m6809_dasm_byte(unsigned int byte, unsigned int pc);
#else
# define m6809_dasm_reset(...)
# define m6809_dasm_byte(...)
#endif

#endif  /* __M6809_DASM_H__ */
