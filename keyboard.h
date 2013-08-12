/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_KEYBOARD_H_
#define XROAR_KEYBOARD_H_

#define NUM_KEYMAPS   (2)
#define KEYMAP_DRAGON (0)
#define KEYMAP_COCO   (1)

/* A keymap describes the keyboard matrix of an emulated machine, as an array
 * of matrix crosspoints indexed by the ASCII value of the key label.  A
 * crosspoint is an (column,row) tuple, each numbered 0 to 7, with (8,8)
 * reserved to indicate no key.
 *
 * All unshifted key labels representable in ASCII are included, plus the lower
 * case equivalents of the alphabetic keys.  Backtick is mapped to CLEAR.  The
 * following keymap indices without obvious ASCII values are also defined: */

#define KEYMAP_BREAK (27)
#define KEYMAP_UP    (94)
#define KEYMAP_LEFT  (8)
#define KEYMAP_RIGHT (9)
#define KEYMAP_DOWN  (10)
#define KEYMAP_ENTER (13)
#define KEYMAP_CLEAR (12)
#define KEYMAP_SHIFT (0)

typedef struct { unsigned col, row; } Keymap[128];
extern Keymap keymap;

#define IS_DRAGON_KEYMAP (xroar_machine_config->keymap == KEYMAP_DRAGON)
#define IS_COCO_KEYMAP (xroar_machine_config->keymap == KEYMAP_COCO)

/* These contain masks to be applied when the corresponding row/column is held
 * low.  eg, if row 1 is outputting a 0 , keyboard_column[1] will be applied on
 * column reads. */

extern unsigned keyboard_column[9];
extern unsigned keyboard_row[9];

/* Press or release a key at the the matrix position (col,row). */

static inline void KEYBOARD_PRESS_MATRIX(int col, int row) {
	keyboard_column[col] &= ~(1<<(row));
	keyboard_row[row] &= ~(1<<(col));
}

static inline void KEYBOARD_RELEASE_MATRIX(int col, int row) {
	keyboard_column[col] |= 1<<(row);
	keyboard_row[row] |= 1<<(col);
}

/* Press or release a key from the current keymap. */

static inline void KEYBOARD_PRESS(int s) {
	KEYBOARD_PRESS_MATRIX(keymap[s].col, keymap[s].row);
}

static inline void KEYBOARD_RELEASE(int s) {
	KEYBOARD_RELEASE_MATRIX(keymap[s].col, keymap[s].row);
}

/* Shift and clear keys are at the same matrix point in both Dragon & CoCo
 * keymaps; indirection through the keymap can be bypassed. */

#define KEYBOARD_PRESS_CLEAR KEYBOARD_PRESS_MATRIX(1,6)
#define KEYBOARD_RELEASE_CLEAR KEYBOARD_RELEASE_MATRIX(1,6)
#define KEYBOARD_PRESS_SHIFT KEYBOARD_PRESS_MATRIX(7,6)
#define KEYBOARD_RELEASE_SHIFT KEYBOARD_RELEASE_MATRIX(7,6)

void keyboard_init(void);
void keyboard_set_keymap(int map);

void keyboard_update(void);
void keyboard_unicode_press(unsigned unicode);
void keyboard_unicode_release(unsigned unicode);
void keyboard_queue_basic(const char *s);

#endif  /* XROAR_KEYBOARD_H_ */
