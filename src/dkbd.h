/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

/* Emulated keyboard symbols. */

#ifndef XROAR_DKBD_H_
#define XROAR_DKBD_H_

#include <stdint.h>

enum {
	// Dragon & CoCo 1/2
	DSCAN_0 = 0x00,
	DSCAN_1 = 0x01,
	DSCAN_2 = 0x02,
	DSCAN_3 = 0x03,
	DSCAN_4 = 0x04,
	DSCAN_5 = 0x05,
	DSCAN_6 = 0x06,
	DSCAN_7 = 0x07,
	DSCAN_8 = 0x08,
	DSCAN_9 = 0x09,
	DSCAN_COLON = 0x0a,
	DSCAN_SEMICOLON = 0x0b,
	DSCAN_COMMA = 0x0c,
	DSCAN_MINUS = 0x0d,
	DSCAN_FULL_STOP = 0x0e,
	DSCAN_SLASH = 0x0f,
	DSCAN_AT = 0x10,
	DSCAN_A = 0x11,
	DSCAN_B = 0x12,
	DSCAN_C = 0x13,
	DSCAN_D = 0x14,
	DSCAN_E = 0x15,
	DSCAN_F = 0x16,
	DSCAN_G = 0x17,
	DSCAN_H = 0x18,
	DSCAN_I = 0x19,
	DSCAN_J = 0x1a,
	DSCAN_K = 0x1b,
	DSCAN_L = 0x1c,
	DSCAN_M = 0x1d,
	DSCAN_N = 0x1e,
	DSCAN_O = 0x1f,
	DSCAN_P = 0x20,
	DSCAN_Q = 0x21,
	DSCAN_R = 0x22,
	DSCAN_S = 0x23,
	DSCAN_T = 0x24,
	DSCAN_U = 0x25,
	DSCAN_V = 0x26,
	DSCAN_W = 0x27,
	DSCAN_X = 0x28,
	DSCAN_Y = 0x29,
	DSCAN_Z = 0x2a,
	DSCAN_UP = 0x2b,
	DSCAN_DOWN = 0x2c,
	DSCAN_LEFT = 0x2d,
	DSCAN_RIGHT = 0x2e,
	DSCAN_SPACE = 0x2f,
	DSCAN_ENTER = 0x30,
	DSCAN_CLEAR = 0x31,
	DSCAN_BREAK = 0x32,
	DSCAN_SHIFT = 0x37,

	// CoCo 3
	DSCAN_ALT = 0x33,
	DSCAN_CTRL = 0x34,
	DSCAN_F1 = 0x35,
	DSCAN_F2 = 0x36,

	DSCAN_INVALID = 0x3f,
};

/* Unicode translations are maintained in a simple array for the Basic Latin
 * and Latin-1 Supplement ranges (i.e., 0 - 255).  The following keys are given
 * otherwise unused values in the control character range: */

enum {
	DKBD_U_CAPS_LOCK = 0x01,  // Start of Header
	DKBD_U_CONTROL = 0x0e,  // Shift Out
	DKBD_U_ALT = 0x0f,  // Shift In
	DKBD_U_F1 = 0x11,  // Device Control 1
	DKBD_U_F2 = 0x12,  // Device Control 2
	DKBD_U_ERASE_LINE = 0x15,  // Negative Acknowledgement
	DKBD_U_BREAK = 0x1b,  // Escape
};

struct dkbd_matrix_point {
	uint8_t row;
	uint8_t col;
};

#define DK_MOD_SHIFT (1 << 0)
#define DK_MOD_UNSHIFT (1 << 1)
#define DK_MOD_CLEAR (1 << 2)

struct dkey_chord {
	int8_t dk_key;
	uint8_t dk_mod;
};

#define DKBD_POINT_TABLE_SIZE (0x40)
#define DKBD_U_TABLE_SIZE (0x0100)

enum dkbd_layout {
	dkbd_layout_dragon,
	dkbd_layout_coco,
	dkbd_layout_dragon200e,
	dkbd_layout_coco3,
};

struct dkbd_map {
	enum dkbd_layout layout;
	struct dkbd_matrix_point point[DKBD_POINT_TABLE_SIZE];
	struct dkey_chord unicode_to_dkey[DKBD_U_TABLE_SIZE];
};

void dkbd_map_init(struct dkbd_map *map, enum dkbd_layout);

#endif  /* XROAR_DKBD_H_ */
