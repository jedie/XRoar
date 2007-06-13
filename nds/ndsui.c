/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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

struct ndsui_component *ndsui_new_component(int x, int y, int w, int h) {
	struct ndsui_component *new;
	new = malloc(sizeof(struct ndsui_component));
	if (new == NULL) return NULL;
	memset(new, 0, sizeof(struct ndsui_component));
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
	if (c->visible) ndsgfx_fillrect(c->x, c->y, c->w, c->h, 0);
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
