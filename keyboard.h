/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_KEYBOARD_H_
#define XROAR_KEYBOARD_H_

#include "types.h"

#define NUM_KEYMAPS   (2)
#define KEYMAP_DRAGON (0)
#define KEYMAP_COCO   (1)

typedef struct { unsigned int col, row; } Keymap[128];
extern Keymap keymap;

#define IS_DRAGON_KEYMAP (running_config.keymap == KEYMAP_DRAGON)
#define IS_COCO_KEYMAP (running_config.keymap == KEYMAP_COCO)

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

#define KEYBOARD_QUEUE(c) do { \
		keyboard_buffer[keyboard_buffer_next] = c; \
		keyboard_buffer_next = (keyboard_buffer_next + 1) % sizeof(keyboard_buffer); \
	} while (0)
#define KEYBOARD_HASQUEUE (keyboard_buffer_next != keyboard_buffer_cursor)
#define KEYBOARD_DEQUEUE() do { \
		keyboard_buffer_cursor = (keyboard_buffer_cursor + 1) % sizeof(keyboard_buffer); \
	} while (0)

extern unsigned int keyboard_buffer[256];
extern int keyboard_buffer_next, keyboard_buffer_cursor;

void keyboard_init(void);
void keyboard_set_keymap(int map);

void keyboard_column_update(void);
void keyboard_row_update(void);
void keyboard_queue_string(const char *s);
void keyboard_queue(unsigned int c);
void keyboard_unicode_press(unsigned int unicode);
void keyboard_unicode_release(unsigned int unicode);

#endif  /* XROAR_KEYBOARD_H_ */
