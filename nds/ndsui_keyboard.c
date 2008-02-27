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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

struct keydata {
	int xoffset, width;
	int sym;
};

static struct keydata keys[5][14] = { 
	{ {5,7,49}, {13,7,50}, {21,7,51}, {29,7,52}, {37,7,53}, {45,7,54}, {53,7,55}, {61,7,56}, {69,7,57}, {77,7,48}, {85,7,58}, {93,7,45}, {101,7,27}, {255,0,0} },
	{ {1,7,94}, {9,7,113}, {17,7,119}, {25,7,101}, {33,7,114}, {41,7,116}, {49,7,121}, {57,7,117}, {65,7,105}, {73,7,111}, {81,7,112}, {89,7,64}, {97,7,8}, {105,7,9} },
	{ {3,7,10}, {11,7,97}, {19,7,115}, {27,7,100}, {35,7,102}, {43,7,103}, {51,7,104}, {59,7,106}, {67,7,107}, {75,7,108}, {83,7,59}, {91,15,13}, {107,7,12}, {255,0,0} },
	{ {3,11,0}, {15,7,122}, {23,7,120}, {31,7,99}, {39,7,118}, {47,7,98}, {55,7,110}, {63,7,109}, {71,7,44}, {79,7,46}, {87,7,47}, {95,11,0}, {255,0,0}, {255,0,0} }, 
	{ {23,63,32}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0} }
};

/**************************************************************************/

struct keyboard_data {
	struct keydata *current;
	int keyy;
	int shifted;
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

struct ndsui_component *new_ndsui_keyboard(int x, int y) {
	struct ndsui_component *new;
	struct keyboard_data *data;
	new = ndsui_new_component(x, y, 230, 82);
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

	data->current = NULL;
	data->shifted = 0;

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

	if (data->current) return;
	x -= self->x;
	y -= self->y;
	if (y < 2 || y > 81) return;

	data->keyy = (y - 2) >> 4;
	for (i = 0; i < 14; i++) {
		if (keys[data->keyy][i].width > 0 && x >= (keys[data->keyy][i].xoffset*2) && x <= ((keys[data->keyy][i].xoffset*2) + (keys[data->keyy][i].width*2))) {
			data->current = &keys[data->keyy][i];
		}
	}
	if (!data->current) return;

	if (data->current->sym == 0) {
		data->shifted ^= 1;
		show(self);
		if (data->shift_callback) {
			data->shift_callback(data->shifted);
		}
		return;
	}
	highlight_key(self);
	if (data->keypress_callback) {
		data->keypress_callback(data->current->sym);
	}
}

static void pen_up(struct ndsui_component *self) {
	struct keyboard_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (data->current) {
		if (data->current->sym != 0) {
			highlight_key(self);
			if (data->keyrelease_callback) {
				data->keyrelease_callback(data->current->sym);
			}
		}
		data->current = NULL;
	}
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
	int x = self->x + (data->current->xoffset * 2);
	int y = self->y + (data->keyy * 16) + 2;
	int w = (data->current->width * 2) + 1;
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
