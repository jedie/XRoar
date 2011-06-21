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
#include "nds/ndsui_filelist.h"

#define FONT_WIDTH 6
#define FONT_HEIGHT 9

struct file_info {
	char *filename;
	mode_t mode;
};

struct file_list_data {
	int w, h;  /* in characters */
	char **extensions;
	int offset;
	int num_slots;
	int num_files;
	struct file_info *files;
	int selected_file;
	void (*num_files_callback)(int);
	void (*visible_callback)(int);
	void (*file_select_callback)(const char *);
};

static void show(struct ndsui_component *self);
static void resize(struct ndsui_component *self, int w, int h);
static void pen_down(struct ndsui_component *self, int x, int y);
static void destroy(struct ndsui_component *self);

/**************************************************************************/

static char fn_buf[MAXPATHLEN];

static void get_directory(struct ndsui_component *self);
static char **string_array_copy(const char **array);
static void string_array_free(char **array);

/**************************************************************************/

struct ndsui_component *new_ndsui_filelist(int x, int y, int w, int h) {
	struct ndsui_component *new;
	struct file_list_data *data;

	new = ndsui_new_component(x, y, w, h);
	if (new == NULL) return NULL;
	data = malloc(sizeof(struct file_list_data));
	if (data == NULL) {
		free(new);
		return NULL;
	}
	memset(data, 0, sizeof(struct file_list_data));

	new->show = show;
	new->resize = resize;
	new->pen_down = pen_down;
	new->destroy = destroy;
	new->data = data;

	data->w = w / FONT_WIDTH;
	data->h = h / FONT_HEIGHT;
	data->offset = 0;
	data->num_slots = 0;
	data->num_files = 0;
	data->selected_file = -1;
	return new;
}

void ndsui_filelist_open(struct ndsui_component *self, const char *path, const char **extensions) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (data->visible_callback)
		data->visible_callback(data->h);
	if (data->extensions) {
		string_array_free(data->extensions);
	}
	data->extensions = string_array_copy(extensions);
	chdir(path);
	data->offset = 0;
	data->selected_file = -1;
	get_directory(self);
	if (self->visible) show(self);
}

void ndsui_filelist_close(struct ndsui_component *self) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (data->extensions) {
		string_array_free(data->extensions);
	}
	if (data->files) {
		int i;
		for (i = 0; i < data->num_slots; i++) {
			if (data->files[i].filename) {
				free(data->files[i].filename);
			}
		}
		free(data->files);
		data->num_files = 0;
		data->num_slots = 0;
		data->files = NULL;
	}
	if (self->visible) show(self);
}

char *ndsui_filelist_filename(struct ndsui_component *self) {
	char *copy = NULL;
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return NULL;
	data = self->data;
	if (data->selected_file != -1)
		copy = strdup(data->files[data->selected_file].filename);
	return copy;
}

void ndsui_filelist_set_offset(struct ndsui_component *self, int offset) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	if (offset < 0) offset = 0;
	data->offset = offset;
	if (self->visible) show(self);
}

void ndsui_filelist_num_files_callback(struct ndsui_component *self, void (*func)(int)) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->num_files_callback = func;
}

void ndsui_filelist_visible_callback(struct ndsui_component *self, void (*func)(int)) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->visible_callback = func;
}

void ndsui_filelist_file_select_callback(struct ndsui_component *self, void (*func)(const char *)) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->file_select_callback = func;
}

void ndsui_filelist_search_string(struct ndsui_component *self, char *str) {
	struct file_list_data *data;
	int j, len;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	len = strlen(str);
	for (j = 0; j < data->num_files; j++) {
		if (strncasecmp(data->files[j].filename, str, len) == 0) {
			data->selected_file = j;
			ndsui_filelist_set_offset(self, j - data->h / 2);
			return;
		}
	}
}

/**************************************************************/

static void show(struct ndsui_component *self) {
	struct file_list_data *data;
	int j;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	ndsgfx_fillrect(self->x, self->y, self->w, self->h, NDS_BLACK);
	for (j = 0; j < data->h; j++) {
		int num = data->offset + j;
		if (num == data->selected_file) {
			nds_set_text_colour(NDS_BLACK, NDS_WHITE);
		} else {
			nds_set_text_colour(NDS_WHITE, NDS_BLACK);
		}
		if (num < data->num_files && data->files[num].filename) {
			if (data->files[num].mode & S_IFDIR) {
				snprintf(fn_buf, data->w + 1, "<%s>", data->files[num].filename);
			} else {
				snprintf(fn_buf, data->w + 1, "%s", data->files[num].filename);
			}
			nds_print_string(self->x, self->y + j * 9, fn_buf);
		}
	}
}

static void resize(struct ndsui_component *self, int w, int h) {
	struct file_list_data *data;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	data->w = w / FONT_WIDTH;
	data->h = h / FONT_HEIGHT;
	if (data->visible_callback)
		data->visible_callback(data->h);
}

static void pen_down(struct ndsui_component *self, int x, int y) {
	struct file_list_data *data;
	int sel;
	(void)x;
	if (self == NULL || self->data == NULL) return;
	data = self->data;
	sel = data->offset + ((y - self->y) / FONT_HEIGHT);
	if (sel >= data->num_files) return;
	if (sel == data->selected_file) {
		if (data->files[sel].mode & S_IFDIR) {
			chdir(data->files[sel].filename);
			get_directory(self);
		}
		data->selected_file = -1;
		if (data->file_select_callback)
			data->file_select_callback(NULL);
	} else {
		data->selected_file = sel;
		if (data->file_select_callback)
			data->file_select_callback(data->files[sel].filename);
	}
	show(self);
}

static void destroy(struct ndsui_component *self) {
	struct file_list_data *data;
	if (!self || !self->data) return;
	data = self->data;
	ndsui_filelist_close(self);
	free(data);
	self->data = NULL;
}

/**************************************************************************/

static int compar_dirents(const void *aa, const void *bb) {
	const struct file_info *a = aa;
	const struct file_info *b = bb;
	if (a == NULL && b == NULL) return 0;
	if (a == NULL) return -1;
	if (b == NULL) return 1;
	if ((a->mode & S_IFDIR) && !(b->mode & S_IFDIR)) return -1;
	if (!(a->mode & S_IFDIR) && (b->mode & S_IFDIR)) return 1;
	return strcasecmp(a->filename, b->filename);
}

static void get_directory(struct ndsui_component *self) {
	struct stat stat_buf;
	DIR_ITER *dir;
	struct file_list_data *data;
	if (!self || !self->data) return;
	data = self->data;
	if (data->num_slots > 0 && data->files) {
		int slot;
		for (slot = 0; slot < data->num_slots; slot++) {
			if (data->files[slot].filename) {
				free(data->files[slot].filename);
				data->files[slot].filename = NULL;
			}
		}
	}
	data->num_files = 0;
	dir = diropen(".");
	if (dir == NULL) return;
	while (dirnext(dir, fn_buf, &stat_buf) == 0) {
		int slot = data->num_files;
		if (slot >= data->num_slots) {
			int i = data->num_slots + 16;
			struct file_info *new_files = realloc(data->files, i * sizeof(struct file_info));
			if (new_files == NULL)
				break;
			if (data->num_files_callback)
				data->num_files_callback(slot + 1);
			data->files = new_files;
			data->num_slots = i;
		}
		data->files[slot].filename = strdup(fn_buf);
		if (data->files[slot].filename == NULL)
			break;
		data->files[slot].mode = stat_buf.st_mode;
		data->num_files = slot + 1;
	}
	dirclose(dir);
	qsort(data->files, data->num_files, sizeof(struct file_info), compar_dirents);
	if (data->num_files_callback)
		data->num_files_callback(data->num_files);
}

static char **string_array_copy(const char **array) {
	char **copy;
	int i;
	if (array == NULL) return NULL;
	for (i = 0; array[i]; i++);
	copy = malloc((i + 1) * sizeof(char *));
	for (i = 0; array[i]; i++) {
		copy[i] = strdup(array[i]);
	}
	return copy;
}

static void string_array_free(char **array) {
	int i;
	for (i = 0; array[i]; i++) {
		free(array[i]);
		array[i] = NULL;
	}
	free(array);
}
