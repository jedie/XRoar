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
#include "nds/ndsui_scrollbar.h"

struct scrollbar_data {
	int total;
	int visible, visible_h;
	int current, current_h;
	void (*offset_callback)(int new);
};

static void show(struct ndsui_component *self);
static void pen_down(struct ndsui_component *self, int x, int y);
static void destroy(struct ndsui_component *self);

/**************************************************************************/

/**************************************************************************/

struct ndsui_component *new_ndsui_scrollbar(int x, int y, int w, int h) {
	struct ndsui_component *new;
	struct scrollbar_data *data;
	if (w < 1 || h < 1)
		return NULL;
	new = ndsui_new_component(x,y,w,h);
	if (new == NULL) return NULL;
	data = malloc(sizeof(struct scrollbar_data));
	if (data == NULL) {
		free(new);
		return NULL;
	}

	new->show = show;
	new->pen_down = pen_down;
	new->pen_move = pen_down;
	new->destroy = destroy;
	new->data = data;

	data->total = 1;
	data->visible = 1;
	data->visible_h = h;
	data->current = 0;
	data->current_h = 0;

	return new;
}

void ndsui_scrollbar_offset_callback(struct ndsui_component *self, void (*func)(int new)) {
	struct scrollbar_data *data;
	if (self == NULL) return;
	data = self->data;
	data->offset_callback = func;
}

void ndsui_scrollbar_set_total(struct ndsui_component *self, int total) {
	struct scrollbar_data *data;
	if (self == NULL) return;
	data = self->data;
	if (total < 1) return;
	data->total = total;
	if (data->visible >= data->total) {
		data->visible_h = self->h;
	} else {
		data->visible_h = (data->visible * self->h) / data->total;
	}
	if (self->visible) show(self);
}

void ndsui_scrollbar_set_visible(struct ndsui_component *self, int visible) {
	struct scrollbar_data *data;
	if (self == NULL) return;
	data = self->data;
	if (visible < 1) return;
	data->visible = visible;
	if (data->visible >= data->total) {
		data->visible_h = self->h;
	} else {
		data->visible_h = (data->visible * self->h) / data->total;
	}
	if (data->visible_h > self->h) {
		data->visible_h = self->h;
	}
	if (self->visible) show(self);
}

/**************************************************************************/

static void show(struct ndsui_component *self) {
	struct scrollbar_data *data;
	if (self == NULL) return;
	data = self->data;
	if ((data->current_h + data->visible_h) > self->h) {
		ndsgfx_fillrect(self->x, self->y, self->w, self->h, NDS_GREY20);
		return;
	}
	if (data->current_h > 0)
		ndsgfx_fillrect(self->x, self->y, self->w, data->current_h - 1, NDS_GREY20);
	ndsgfx_fillrect(self->x, self->y + data->current_h, self->w, data->visible_h, NDS_GREY60);
	if ((data->current_h + data->visible_h) < self->h)
		ndsgfx_fillrect(self->x, self->y + data->current_h + data->visible_h, self->w, self->h - (data->current_h + data->visible_h), NDS_GREY20);
}

static void pen_down(struct ndsui_component *self, int x, int y) {
	struct scrollbar_data *data;
	int new_cur;
	(void)x;
	if (self == NULL) return;
	data = self->data;
	new_cur = ((y - self->y) * data->total) / self->h;
	new_cur -= data->visible / 2;
	if (new_cur >= (data->total - data->visible))
		new_cur = data->total - data->visible;
	if (new_cur < 0) new_cur = 0;
	if (new_cur == data->current) return;
	data->current = new_cur;
	data->current_h = (new_cur * self->h) / data->total;
	show(self);
	if (data->offset_callback)
		data->offset_callback(new_cur);
}

static void destroy(struct ndsui_component *self) {
	struct scrollbar_data *data;
	if (!self || !self->data) return;
	data = self->data;
	free(data);
	self->data = NULL;
}
