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
#include "nds/ndsui_button.h"

#define FONT_WIDTH 6
#define FONT_HEIGHT 9

struct button_data {
	char *label;
	int label_size;
	int is_toggle;
	int pressed;
	void (*press_callback)(int);
	void (*release_callback)(int);
};

static void show(struct ndsui_component *self);
static void pen_down(struct ndsui_component *self, int x, int y);
static void pen_move(struct ndsui_component *self, int x, int y);
static void pen_up(struct ndsui_component *self);
static void destroy(struct ndsui_component *self);

/**************************************************************************/

struct ndsui_component *new_ndsui_button(int x, int y, int w, const char *label, int is_toggle) {
	struct ndsui_component *new;
	struct button_data *data;
	int lsize;
	int h = FONT_HEIGHT + 3;
	char *new_label;

	if (w >= 12) {
		lsize = w / 6;
	} else {
		lsize = strlen(label) + 1;
		w = lsize * FONT_WIDTH;
	}
	if (lsize < 2) return NULL;
	new_label = malloc(lsize);
	if (new_label == NULL) return NULL;

	new = ndsui_new_component(x, y, w, h);
	if (new == NULL) return NULL;
	data = malloc(sizeof(struct button_data));
	if (data == NULL) {
		free(new);
		return NULL;
	}
	memset(data, 0, sizeof(struct button_data));

	new->show = show;
	new->pen_down = pen_down;
	new->pen_move = pen_move;
	new->pen_up = pen_up;
	new->destroy = destroy;
	new->data = data;

	if (label) {
		strncpy(new_label, label, lsize);
	} else {
		memset(new_label, 0, lsize);
	}
	data->label = new_label;
	data->label_size = lsize;
	data->is_toggle = is_toggle;
	data->pressed = 0;

	return new;
}

void ndsui_button_press_callback(struct ndsui_component *self, void (*func)(int)) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->press_callback = func;
}

void ndsui_button_release_callback(struct ndsui_component *self, void (*func)(int)) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->release_callback = func;
}

void ndsui_button_set_label(struct ndsui_component *self, const char *label) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	strncpy(data->label, label, data->label_size);
	if (self->visible)
		show(self);
}

void ndsui_button_set_state(struct ndsui_component *self, int pressed) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->pressed = pressed;
	show(self);
}

/**************************************************************************/

static void show(struct ndsui_component *self) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	if (data->pressed) {
		ndsgfx_fillrect(self->x+1, self->y+1, self->w-2, self->h-2, NDS_GREY20);
		nds_set_text_colour(NDS_WHITE, NDS_GREY20);
	} else {
		ndsgfx_fillrect(self->x, self->y, self->w, self->h, NDS_WHITE);
		nds_set_text_colour(NDS_GREY20, NDS_WHITE);
	}
	nds_print_string(self->x + 3, self->y + 2, data->label);
}

static void pen_down(struct ndsui_component *self, int x, int y) {
	struct button_data *data;
	(void)x;
	(void)y;
	if (self == NULL) return;
	data = self->data;
	if (data->is_toggle)
		data->pressed = !data->pressed;
	else
		data->pressed = 1;
	show(self);
	if (data->pressed && data->press_callback)
		data->press_callback(self->id);
	if (!data->pressed && data->release_callback)
		data->release_callback(self->id);
}

static void pen_move(struct ndsui_component *self, int x, int y) {
	struct button_data *data;
	int old_pressed;
	if (self == NULL) return;
	data = self->data;
	if (data->is_toggle)
		return;
	old_pressed = data->pressed;
	if (x >= self->x && x < (self->x + self->w)
			&& y >= self->y && y < (self->y + self->h)) {
		data->pressed = 1;
	} else {
		data->pressed = 0;
	}
	if (old_pressed != data->pressed)
		show(self);
}

static void pen_up(struct ndsui_component *self) {
	struct button_data *data;
	int old_pressed;
	if (self == NULL) return;
	data = self->data;
	if (data->is_toggle)
		return;
	old_pressed = data->pressed;
	data->pressed = 0;
	show(self);
	if (old_pressed && data->release_callback)
		data->release_callback(self->id);
}

static void destroy(struct ndsui_component *self) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	if (data->label)
		free(data->label);
	free(self->data);
	self->data = NULL;
}
