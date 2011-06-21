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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>
#include <fat.h>
#include <sys/dir.h>

#include "types.h"
#include "logging.h"
#include "module.h"
#include "nds/ndsui.h"
#include "nds/ndsgfx.h"

static struct ndsui_component *component_list = NULL;
static struct ndsui_component *current_component = NULL;

/* All components have a unique id, but it's not used internally */
static unsigned int next_id = 0;

void ndsui_add_component(struct ndsui_component *c) {
	if (c == NULL) return;
	/* c->next being set implies component is already on the list */
	if (c->next != NULL) return;
	c->next = component_list;
	component_list = c;
}

void ndsui_show_all_components(void) {
	struct ndsui_component *c;
	for (c = component_list; c; c = c->next) {
		ndsui_show_component(c);
	}
}

void ndsui_clear_component_list(void) {
	struct ndsui_component **cp = &component_list;
	while (*cp) {
		struct ndsui_component **next = &((*cp)->next);
		ndsui_hide_component(*cp);
		*cp = NULL;
		cp = next;
	}
}

void ndsui_pen_down(int x, int y) {
	struct ndsui_component *c;
	for (c = component_list; c; c = c->next) {
		if (c->visible && x >= c->x && x < (c->x+c->w) && y >= c->y && y < (c->y+c->h)) {
			current_component = c;
		}
	}
	if (current_component) {
		if (current_component->pen_down) {
			current_component->pen_down(current_component, x, y);
		}
	}
}

void ndsui_pen_move(int x, int y) {
	if (current_component) {
		if (current_component->pen_move) {
			current_component->pen_move(current_component, x, y);
		}
	}
}

void ndsui_pen_up(void) {
	if (current_component) {
		if (current_component->pen_up) {
			current_component->pen_up(current_component);
		}
		current_component = NULL;
	}
}

struct ndsui_component *ndsui_new_component(int x, int y, int w, int h) {
	struct ndsui_component *new;
	new = malloc(sizeof(struct ndsui_component));
	if (new == NULL) return NULL;
	memset(new, 0, sizeof(struct ndsui_component));
	new->id = next_id++;
	new->x = x;
	new->y = y;
	new->w = w;
	new->h = h;
	new->visible = 0;
	return new;
}

void ndsui_show_component(struct ndsui_component *c) {
	c->visible = 1;
	ndsui_draw_component(c);
}

void ndsui_hide_component(struct ndsui_component *c) {
	ndsui_undraw_component(c);
	c->visible = 0;
}

void ndsui_draw_component(struct ndsui_component *c) {
	if (c->visible && c->show) c->show(c);
}

void ndsui_undraw_component(struct ndsui_component *c) {
	if (c->visible) ndsgfx_fillrect(c->x, c->y, c->w, c->h, NDS_DARKPURPLE);
}

void ndsui_resize_component(struct ndsui_component *c, int w, int h) {
	int visible;
	if (c == NULL) return;
	visible = c->visible;
	ndsui_undraw_component(c);
	c->w = w;
	c->h = h;
	if (c->resize) c->resize(c, w, h);
	ndsui_draw_component(c);
}
