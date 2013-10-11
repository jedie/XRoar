/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "dkbd.h"

/* This could be a lot simpler, but at some point I want to make these mappings
 * completely user definable, so I'm abstracting the definitions a little. */

struct dkey_chord_mapping {
	unsigned unicode;
	struct dkey_chord chord;
};

/* A map variant consists of the base layout (dragon or coco ONLY), and then
 * additional mappings. */

struct dkbd_layout_variant {
	enum dkbd_layout base_layout;
	int num_chord_mappings;
	struct dkey_chord_mapping *chord_mappings;
};

static struct dkey_chord_mapping dragon_chord_mappings[] = {
	{ DKBD_U_CAPS_LOCK, { DSCAN_0, DK_MOD_SHIFT } },
	{ '@', { DSCAN_AT, DK_MOD_UNSHIFT } },
	{ '\\', { DSCAN_INVALID, DK_MOD_SHIFT|DK_MOD_CLEAR } },
	{ '[', { DSCAN_DOWN, DK_MOD_SHIFT} },
	{ ']', { DSCAN_RIGHT, DK_MOD_SHIFT} },
};

static struct dkey_chord_mapping dragon200e_chord_mappings[] = {
	{ 0xc7, { DSCAN_0, DK_MOD_SHIFT } },  // 'Ç'
	{ 0xe7, { DSCAN_0, DK_MOD_SHIFT } },  // 'ç'
	{ 0xdc, { DSCAN_BREAK, DK_MOD_SHIFT } },  // 'Ü'
	{ 0xfc, { DSCAN_BREAK, DK_MOD_SHIFT } },  // 'ü'
	{ ';', { DSCAN_AT, DK_MOD_UNSHIFT } },
	{ '+', { DSCAN_AT, DK_MOD_SHIFT } },
	{ 0xcf, { DSCAN_RIGHT, DK_MOD_UNSHIFT } },  // 'Î'
	{ 0xef, { DSCAN_RIGHT, DK_MOD_UNSHIFT } },  // 'î'
	{ 0xbf, { DSCAN_RIGHT, DK_MOD_SHIFT } },  // '¿'
	{ 0xc3, { DSCAN_DOWN, DK_MOD_UNSHIFT } },  // 'Ã'
	{ 0xe3, { DSCAN_DOWN, DK_MOD_UNSHIFT } },  // 'ã'
	{ 0xa1, { DSCAN_DOWN, DK_MOD_SHIFT } },  // '¡'
	{ 0xf1, { DSCAN_SEMICOLON, DK_MOD_UNSHIFT } },  // 'ñ'
	{ 0xd1, { DSCAN_SEMICOLON, DK_MOD_SHIFT } },  // 'Ñ'
	{ DKBD_U_CAPS_LOCK, { DSCAN_ENTER, DK_MOD_SHIFT } },
	{ '@', { DSCAN_CLEAR, DK_MOD_SHIFT } },
	{ 0xa7, { DSCAN_SPACE, DK_MOD_SHIFT } },  // '§'
};

static struct dkey_chord_mapping coco3_chord_mappings[] = {
	{ DKBD_U_CAPS_LOCK, { DSCAN_0, DK_MOD_SHIFT } },
	// TODO: ALT
	{ '@', { DSCAN_AT, DK_MOD_UNSHIFT } },
	{ '\\', { DSCAN_INVALID, DK_MOD_SHIFT|DK_MOD_CLEAR } },
	// TODO: CONTROL
	{ DKBD_U_F1, { DSCAN_F1, 0 } },
	{ DKBD_U_F2, { DSCAN_F2, 0 } },
};

#define CMAPPING(m) .num_chord_mappings = G_N_ELEMENTS(m), .chord_mappings = (m)

static struct dkbd_layout_variant dkbd_layout_variants[] = {
	{ .base_layout = dkbd_layout_dragon, CMAPPING(dragon_chord_mappings), },
	{ .base_layout = dkbd_layout_coco, CMAPPING(dragon_chord_mappings), },
	{ .base_layout = dkbd_layout_dragon, CMAPPING(dragon200e_chord_mappings), },
	{ .base_layout = dkbd_layout_coco, CMAPPING(coco3_chord_mappings), },
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/* Key values are chosen so that they directly encode the crosspoint locations
 * for a normal Dragon.  CoCo map requires a small translation. */

void dkbd_map_init(struct dkbd_map *map, enum dkbd_layout layout) {
	assert(layout >= 0 && layout < G_N_ELEMENTS(dkbd_layout_variants));
	struct dkbd_layout_variant *variant = &dkbd_layout_variants[layout];
	map->layout = layout;

	/* Populate the matrix crosspoint map */

	// Clear table
	for (int i = 0; i < 56; i++) {
		unsigned col = i & 7;
		unsigned row = (i >> 3) & 7;
		if (variant->base_layout == dkbd_layout_coco) {
			if (row != 6)
				row = (row + 4) % 6;
		}
		map->point[i] = (struct dkbd_matrix_point){row, col};
	}
	if (layout != dkbd_layout_coco3) {
		// Unmap CoCo 3 extended keys
		for (int i = 0x33; i <= 0x36; i++)
			map->point[i] = (struct dkbd_matrix_point){8, 8};
	}
	for (int i = 0x38; i < DKBD_POINT_TABLE_SIZE; i++)
		map->point[i] = (struct dkbd_matrix_point){8, 8};

	/* Populate the unicode_to_dkey map */

	// Clear table
	for (int i = 0; i < G_N_ELEMENTS(map->unicode_to_dkey); i++) {
		map->unicode_to_dkey[i] = (struct dkey_chord){DSCAN_INVALID, 0};
	}
	// "1!" - "9)", ":*", ";+"
	for (int i = 0; i <= 10; i++) {
		map->unicode_to_dkey['1'+i] = (struct dkey_chord){DSCAN_1+i, DK_MOD_UNSHIFT};
		map->unicode_to_dkey['!'+i] = (struct dkey_chord){DSCAN_1+i, DK_MOD_SHIFT};
	}
	// ",<", "-=", ".>", "/?"
	for (int i = 0; i <= 3; i++) {
		map->unicode_to_dkey[','+i] = (struct dkey_chord){DSCAN_COMMA+i, DK_MOD_UNSHIFT};
		map->unicode_to_dkey['<'+i] = (struct dkey_chord){DSCAN_COMMA+i, DK_MOD_SHIFT};
	}
	// "aA" - "zZ"
	for (int i = 0; i <= 25; i++) {
		map->unicode_to_dkey['a'+i] = (struct dkey_chord){DSCAN_A+i, DK_MOD_UNSHIFT};
		map->unicode_to_dkey['A'+i] = (struct dkey_chord){DSCAN_A+i, DK_MOD_SHIFT};
	}
	// Rest of standard keys
	map->unicode_to_dkey['0'] = (struct dkey_chord){DSCAN_0, DK_MOD_UNSHIFT};
	map->unicode_to_dkey[' '] = (struct dkey_chord){DSCAN_SPACE, 0};
	map->unicode_to_dkey[DKBD_U_BREAK] = (struct dkey_chord){DSCAN_BREAK, 0};
	map->unicode_to_dkey[0x08] = (struct dkey_chord){DSCAN_LEFT, DK_MOD_UNSHIFT};  // BS
	map->unicode_to_dkey[0x09] = (struct dkey_chord){DSCAN_RIGHT, DK_MOD_UNSHIFT};  // HT
	map->unicode_to_dkey[0x0a] = (struct dkey_chord){DSCAN_ENTER, 0};  // LF
	map->unicode_to_dkey[0x0c] = (struct dkey_chord){DSCAN_CLEAR, 0};  // FF
	map->unicode_to_dkey[0x0d] = (struct dkey_chord){DSCAN_ENTER, 0};  // CR
	map->unicode_to_dkey[0x19] = (struct dkey_chord){DSCAN_RIGHT, 0};  // EM
	map->unicode_to_dkey[0x5e] = (struct dkey_chord){DSCAN_UP, DK_MOD_UNSHIFT};  // '^'
	map->unicode_to_dkey[0x5f] = (struct dkey_chord){DSCAN_UP, DK_MOD_SHIFT};  // '_'
	map->unicode_to_dkey[0x7f] = (struct dkey_chord){DSCAN_LEFT, DK_MOD_UNSHIFT};  // DEL
	// Standard extras
	map->unicode_to_dkey[DKBD_U_ERASE_LINE] = (struct dkey_chord){DSCAN_LEFT, DK_MOD_SHIFT};
	map->unicode_to_dkey[0xa3] = (struct dkey_chord){DSCAN_3, DK_MOD_SHIFT};  // '£'
	map->unicode_to_dkey[0xba] = (struct dkey_chord){DSCAN_CLEAR, DK_MOD_UNSHIFT};  // 'º'
	map->unicode_to_dkey[0xaa] = (struct dkey_chord){DSCAN_CLEAR, DK_MOD_SHIFT};  // 'ª'
	// Variant mappings
	for (int i = 0; i < variant->num_chord_mappings; i++) {
		unsigned unicode = variant->chord_mappings[i].unicode;
		map->unicode_to_dkey[unicode] = variant->chord_mappings[i].chord;
	}
}
