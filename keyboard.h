/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
 *
 *  See COPYING for redistribution conditions. */

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "types.h"

typedef struct KeyboardModule KeyboardModule;
struct KeyboardModule {
	KeyboardModule *next;
	char *name;
	char *help;
	int (*init)(void);
	void (*shutdown)(void);
	void (*poll)(void);
};

/* Video modules will probably have a keyboard module preference, so
 * will set this: */
extern KeyboardModule *keyboard_module;

/* These contain masks to be applied when the corresponding row/column is
 * held low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will
 * be applied on column reads */
extern uint_fast8_t keyboard_column[9];
extern uint_fast8_t keyboard_row[9];

#define KEYBOARD_PRESS(s) { \
		keyboard_column[keymap[s].col] &= ~(1<<keymap[s].row); \
		keyboard_row[keymap[s].row] &= ~(1<<keymap[s].col); \
	}
#define KEYBOARD_RELEASE(s) { \
		keyboard_column[keymap[s].col] |= 1<<keymap[s].row; \
		keyboard_row[keymap[s].row] |= 1<<keymap[s].col; \
	}

#define KEYBOARD_HASQUEUE (keyboard_buflast > keyboard_bufcur)
/* For each key, queue: shift up/down as appropriate, key down, key up,
 * shift up */
#define KEYBOARD_QUEUE(c) {  \
		if ((c) & 0x80) \
			*(keyboard_buflast++) = 0; \
		else \
			*(keyboard_buflast++) = 0x80; \
		*(keyboard_buflast++) = (c) & 0x7f; \
		*(keyboard_buflast++) = (c) | 0x80; \
		*(keyboard_buflast++) = 0x80; \
	}

#define KEYBOARD_DEQUEUE(c) { \
		c = *(keyboard_bufcur++); \
		if (keyboard_bufcur >= keyboard_buflast) { \
			keyboard_bufcur = keyboard_buflast = keyboard_buffer; \
		} \
	}

extern uint_fast8_t keyboard_buffer[256];
extern uint_fast8_t *keyboard_bufcur, *keyboard_buflast;

int keyboard_init(void);
void keyboard_shutdown(void);
void keyboard_reset(void);
void keyboard_column_update(void);
void keyboard_row_update(void);
void keyboard_queue_string(char *s);

#endif  /* __KEYBOARD_H__ */
