/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "types.h"

/* These contain masks to be applied when the corresponding row/column is
 * held low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will
 * be applied on column reads */
extern unsigned int keyboard_column[9];
extern unsigned int keyboard_row[9];

#define KEYBOARD_PRESS(s) do { \
		keyboard_column[keymap[s].col] &= ~(1<<keymap[s].row); \
		keyboard_row[keymap[s].row] &= ~(1<<keymap[s].col); \
	} while (0)
#define KEYBOARD_RELEASE(s) do { \
		keyboard_column[keymap[s].col] |= 1<<keymap[s].row; \
		keyboard_row[keymap[s].row] |= 1<<keymap[s].col; \
	} while (0)

#define KEYBOARD_HASQUEUE (keyboard_buflast > keyboard_bufcur)
/* For each key, queue: shift up/down as appropriate, key down, key up,
 * shift up */
#define KEYBOARD_QUEUE(c) do {  \
		if ((c) & 0x80) \
			*(keyboard_buflast++) = 0; \
		else \
			*(keyboard_buflast++) = 0x80; \
		*(keyboard_buflast++) = (c) & 0x7f; \
		*(keyboard_buflast++) = (c) | 0x80; \
		*(keyboard_buflast++) = 0x80; \
	} while (0)

#define KEYBOARD_DEQUEUE(c) do { \
		c = *(keyboard_bufcur++); \
		if (keyboard_bufcur >= keyboard_buflast) { \
			keyboard_bufcur = keyboard_buflast = keyboard_buffer; \
		} \
	} while (0)

extern unsigned int keyboard_buffer[256];
extern unsigned int *keyboard_bufcur, *keyboard_buflast;

void keyboard_init(void);
void keyboard_column_update(void);
void keyboard_row_update(void);
void keyboard_queue_string(const char *s);
void keyboard_queue(uint_least16_t c);

#endif  /* __KEYBOARD_H__ */
