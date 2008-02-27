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
#include "nds/ndsui_button.h"

#define FONT_WIDTH 6
#define FONT_HEIGHT 9

struct button_data {
	char *label;
	int pressed;
	void (*press_callback)(void);
	void (*release_callback)(void);
};

static void show(struct ndsui_component *self);
static void pen_down(struct ndsui_component *self, int x, int y);
static void pen_up(struct ndsui_component *self);
static void destroy(struct ndsui_component *self);

/**************************************************************************/

struct ndsui_component *new_ndsui_button(int x, int y, const char *label) {
	struct ndsui_component *new;
	struct button_data *data;
	int w = (strlen(label) * FONT_WIDTH) + 6;
	int h = FONT_HEIGHT + 3;

	if (label == NULL) return NULL;
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
	new->pen_up = pen_up;
	new->destroy = destroy;
	new->data = data;

	data->label = strdup(label);
	data->pressed = 0;

	return new;
}

void ndsui_button_press_callback(struct ndsui_component *self, void (*func)(void)) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->press_callback = func;
}

void ndsui_button_release_callback(struct ndsui_component *self, void (*func)(void)) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->release_callback = func;
}

/**************************************************************************/

static void show(struct ndsui_component *self) {
	unsigned int fgcolour, bgcolour;
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	if (data->pressed) {
		fgcolour = ~0;
		bgcolour = 0x333333ff;
	} else {
		fgcolour = 0x333333ff;
		bgcolour = ~0;
	}
	ndsgfx_fillrect(self->x, self->y, self->w, self->h, bgcolour);
	nds_set_text_colour(fgcolour, bgcolour);
	nds_print_string(self->x + 3, self->y + 2, data->label);
}

static void pen_down(struct ndsui_component *self, int x, int y) {
	struct button_data *data;
	(void)x;
	(void)y;
	if (self == NULL) return;
	data = self->data;
	data->pressed = 1;
	show(self);
	if (data->press_callback)
		data->press_callback();
}

static void pen_up(struct ndsui_component *self) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	data->pressed = 0;
	show(self);
	if (data->release_callback)
		data->release_callback();
}

static void destroy(struct ndsui_component *self) {
	struct button_data *data;
	if (self == NULL) return;
	data = self->data;
	free(self->data);
	self->data = NULL;
}
