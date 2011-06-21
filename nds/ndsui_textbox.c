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

#include "types.h"
#include "logging.h"
#include "module.h"
#include "nds/ndsgfx.h"
#include "nds/ndsui.h"
#include "nds/ndsui_textbox.h"

#define FONT_WIDTH 6
#define FONT_HEIGHT 9

struct textbox_data {
	char *text;
	int w;
	int max_length;
	void (*press_callback)(void);
	void (*release_callback)(void);
};

static void show(struct ndsui_component *self);
static void destroy(struct ndsui_component *self);

/**************************************************************************/

struct ndsui_component *new_ndsui_textbox(int x, int y, int w, int max_length) {
	struct ndsui_component *new;
	struct textbox_data *data;
	int h = FONT_HEIGHT + 3;

	new = ndsui_new_component(x, y, w, h);
	if (new == NULL) return NULL;
	data = malloc(sizeof(struct textbox_data));
	if (data == NULL) {
		free(new);
		return NULL;
	}
	memset(data, 0, sizeof(struct textbox_data));

	new->show = show;
	new->destroy = destroy;
	new->data = data;

	data->w = w / FONT_WIDTH;
	if (max_length >= 0) {
		data->max_length = max_length + 1;
	} else {
		data->max_length = data->w + 1;
	}
	data->text = malloc(data->max_length);
	data->text[0] = 0;

	return new;
}

void ndsui_textbox_set_text(struct ndsui_component *self, const char *text) {
	struct textbox_data *data;
	if (self == NULL) return;
	data = self->data;
	data->text[0] = 0;
	if (text)
		strncpy(data->text, text, data->max_length);
	show(self);
}

char *ndsui_textbox_get_text(struct ndsui_component *self) {
	struct textbox_data *data;
	if (self == NULL) return NULL;
	data = self->data;
	return data->text;
}

void ndsui_textbox_type_char(struct ndsui_component *self, int c) {
	struct textbox_data *data;
	int length;
	if (self == NULL) return;
	data = self->data;
	length = strlen(data->text);
	if (c == 8 && length > 0) {
			data->text[length-1] = 0;
	}
	if (c >= 32 && (length+1) < data->max_length)  {
		data->text[length] = c;
		data->text[length+1] = 0;
	}
	show(self);
}

/**************************************************************************/

static void show(struct ndsui_component *self) {
	struct textbox_data *data;
	int length;
	if (self == NULL) return;
	data = self->data;
	ndsgfx_fillrect(self->x, self->y, self->w, self->h, NDS_WHITE);
	nds_set_text_colour(NDS_GREY20, NDS_WHITE);
	length = strlen(data->text);
	if (length > data->w) {
		nds_print_string(self->x, self->y + 2, &data->text[length-data->w]);
	} else {
		nds_print_string(self->x, self->y + 2, data->text);
	}
}

static void destroy(struct ndsui_component *self) {
	struct textbox_data *data;
	if (self == NULL) return;
	data = self->data;
	if (data) {
		if (data->text) {
			free(data->text);
			data->text = NULL;
		}
	}
	free(self->data);
	self->data = NULL;
}
