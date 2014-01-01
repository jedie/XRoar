/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_KEYBOARD_H_
#define XROAR_KEYBOARD_H_

#include "dkbd.h"

#define NUM_KEYMAPS   (3)
#define KEYMAP_DRAGON (0)
#define KEYMAP_COCO   (1)
#define KEYMAP_DRAGON200E (2)

extern struct dkbd_map keymap_new;

#define IS_DRAGON_KEYMAP (xroar_machine_config->keymap == KEYMAP_DRAGON)
#define IS_COCO_KEYMAP (xroar_machine_config->keymap == KEYMAP_COCO)

/* These contain masks to be applied when the corresponding row/column is held
 * low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will be applied on
 * column reads. */

extern unsigned keyboard_column[9];
extern unsigned keyboard_row[9];

struct keyboard_state {
	unsigned row_source;
	unsigned row_sink;
	unsigned col_source;
	unsigned col_sink;
};

/* Press or release a key at the the matrix position (col,row). */

static inline void keyboard_press_matrix(int col, int row) {
	keyboard_column[col] &= ~(1<<(row));
	keyboard_row[row] &= ~(1<<(col));
}

static inline void keyboard_release_matrix(int col, int row) {
	keyboard_column[col] |= 1<<(row);
	keyboard_row[row] |= 1<<(col);
}

/* Press or release a key from the current keymap. */

static inline void keyboard_press(int s) {
	keyboard_press_matrix(keymap_new.point[s].col, keymap_new.point[s].row);
}

static inline void keyboard_release(int s) {
	keyboard_release_matrix(keymap_new.point[s].col, keymap_new.point[s].row);
}

/* Shift and clear keys are at the same matrix point in both Dragon & CoCo
 * keymaps; indirection through the keymap can be bypassed. */

#define KEYBOARD_PRESS_CLEAR keyboard_press_matrix(1,6)
#define KEYBOARD_RELEASE_CLEAR keyboard_release_matrix(1,6)
#define KEYBOARD_PRESS_SHIFT keyboard_press_matrix(7,6)
#define KEYBOARD_RELEASE_SHIFT keyboard_release_matrix(7,6)

/* Chord mode affects how special characters are typed (specifically, the
 * backslash character when in translation mode). */
enum keyboard_chord_mode {
	keyboard_chord_mode_dragon_32k_basic,
	keyboard_chord_mode_dragon_64k_basic,
	keyboard_chord_mode_coco_basic
};

void keyboard_init(void);
void keyboard_set_keymap(int map);

void keyboard_set_chord_mode(enum keyboard_chord_mode mode);

void keyboard_read_matrix(struct keyboard_state *);
void keyboard_unicode_press(unsigned unicode);
void keyboard_unicode_release(unsigned unicode);
void keyboard_queue_basic(const uint8_t *s);

#endif  /* XROAR_KEYBOARD_H_ */
