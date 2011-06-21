/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>
#include <fat.h>
#include <sys/dir.h>
#include <sys/unistd.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "nds/ndsgfx.h"
#include "nds/ndsui.h"
#include "nds/ndsui_keyboard.h"

extern Sprite kbd_bin[2];

#define KEY_HEIGHT (15)
#define NUM_KEY_GROUPS (9)

static int row1_keys[13] = { 49, 50, 51, 52, 53, 54, 55, 56, 57, 48, 58, 45, 27 };
static int row2_keys[14] = { 94, 113, 119, 101, 114, 116, 121, 117, 105, 111, 112, 64, 8, 9 };
static int row3_1_keys[11] = { 10, 97, 115, 100, 102, 103, 104, 106, 107, 108, 59 };
static int row3_2_keys[1] = { 13 };
static int row3_3_keys[1] = { 12 };
static int row4_1_keys[1] = { 0 };
static int row4_2_keys[10] = { 122, 120, 99, 118, 98, 110, 109, 44, 46, 47 };
static int row5_keys[1] = { 32 };

static struct key_group {
	int xoffset, yoffset;
	int num_keys;
	int key_width;
	int *key_values;
} key_groups[NUM_KEY_GROUPS] = {
	{ 8, 0, 13, 15, row1_keys },
	{ 0, 16, 14, 15, row2_keys },
	{ 4, 32, 11, 15, row3_1_keys },
	{ 180, 32, 1, 31, row3_2_keys },
	{ 212, 32, 1, 15, row3_3_keys },
	{ 4, 48, 1, 23, row4_1_keys },
	{ 28, 48, 10, 15, row4_2_keys },
	{ 188, 48, 1, 23, row4_1_keys },
	{ 44, 64, 1, 127, row5_keys }
};

/**************************************************************************/

struct keyboard_data {
	int shift_is_toggle;
	int shifted;
	int key_xoffset, key_yoffset;  /* currently pressed key */
	int key_w, key_value;          /* currently pressed key */
	void (*keypress_callback)(int sym);
	void (*keyrelease_callback)(int sym);
	void (*shift_callback)(int state);
};

static void show(struct ndsui_component *self);
static void pen_down(struct ndsui_component *self, int x, int y);
static void pen_up(struct ndsui_component *self);
static void destroy(struct ndsui_component *self);

static void highlight_key(struct ndsui_component *self);

/**************************************************************************/

struct ndsui_component *new_ndsui_keyboard(int x, int y, int shift_is_toggle) {
	struct ndsui_component *new;
	struct keyboard_data *data;
	new = ndsui_new_component(x, y, 227, 79);
	if (new == NULL) return NULL;
	data = malloc(sizeof(struct keyboard_data));
	if (data == NULL) {
		free(new);
		return NULL;
	}

	new->show = show;
	new->pen_down = pen_down;
	new->pen_up = pen_up;
	new->destroy = destroy;
	new->data = data;

	data->shift_is_toggle = shift_is_toggle;
	data->key_value = -1;

	return new;
}

void ndsui_keyboard_keypress_callback(struct ndsui_component *self, void (*func)(int)) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->keypress_callback = func;
}

void ndsui_keyboard_keyrelease_callback(struct ndsui_component *self, void (*func)(int)) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->keyrelease_callback = func;
}

void ndsui_keyboard_shift_callback(struct ndsui_component *self, void (*func)(int)) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->shift_callback = func;
}

void ndsui_keyboard_set_shift_state(struct ndsui_component *self, int state) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (data->shifted != state) {
		data->shifted = state ? 1 : 0;
		show(self);
	}
}

/**************************************************************************/

static void show(struct ndsui_component *self) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	ndsgfx_blit(self->x, self->y, &kbd_bin[data->shifted]);
}

static void pen_down(struct ndsui_component *self, int x, int y) {
	struct keyboard_data *data;
	int i;
	if (self == NULL || self->data == NULL) return;
	data = self->data;

	if (data->key_value >= 0) return;
	x -= self->x;
	y -= self->y;

	for (i = 0; i < NUM_KEY_GROUPS; i++) {
		if (y >= key_groups[i].yoffset
				&& y < (key_groups[i].yoffset + KEY_HEIGHT)
				&& x >= key_groups[i].xoffset
				&& x < (key_groups[i].xoffset + (key_groups[i].num_keys * (key_groups[i].key_width + 1)))) {
			int num = (x - key_groups[i].xoffset) / (key_groups[i].key_width + 1);
			data->key_w = key_groups[i].key_width;
			data->key_xoffset = key_groups[i].xoffset + num * (key_groups[i].key_width + 1);
			data->key_yoffset = key_groups[i].yoffset;
			data->key_value = key_groups[i].key_values[num];
		}
	}

	if (data->key_value < 0) return;

	if (data->key_value == 0) {
		if (data->shift_is_toggle) {
			data->shifted ^= 1;
			show(self);
			if (data->shift_callback) {
				data->shift_callback(data->shifted);
			}
		} else {
			data->shifted = 1;
			show(self);
			if (data->keypress_callback) {
				data->keypress_callback(data->key_value);
			}
		}
		return;
	}
	highlight_key(self);
	if (data->keypress_callback) {
		data->keypress_callback(data->key_value);
	}
}

static void pen_up(struct ndsui_component *self) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (data->key_value < 0) return;

	if (data->key_value == 0) {
		if (!data->shift_is_toggle) {
			data->shifted = 0;
			show(self);
			if (data->keyrelease_callback) {
				data->keyrelease_callback(data->key_value);
			}
		}
	} else {
		highlight_key(self);
		if (data->keyrelease_callback) {
			data->keyrelease_callback(data->key_value);
		}
	}
	data->key_value = -1;
}

static void destroy(struct ndsui_component *self) {
	struct keyboard_data *data;
	if (!self || !self->data) return;
	data = self->data;
	free(data);
	self->data = NULL;
}

static void highlight_key(struct ndsui_component *self) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	int x = self->x + data->key_xoffset;
	int y = self->y + data->key_yoffset;
	int w = data->key_w;
	uint16_t *p = (uint16_t *)BG_GFX_SUB + (y*256) + x;
	int skip = 256 - w;
	int i, j;
	for (j = 15; j; j--) {
		for (i = w; i; i--) {
			*(p++) ^= 0x7fff;
		}
		p += skip;
	}
}
