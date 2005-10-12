/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

#ifdef HAVE_GP32

#include <stdlib.h>
#include <string.h>
#include <gpgraphic.h>

#include "types.h"
#include "gp32/gp32.h"
#include "gp32/gpkeypad.h"
#include "gp32/gpchatboard.h"
#include "ui.h"
#include "machine.h"
#include "snapshot.h"
#include "sound.h"
#include "tape.h"
#include "vdg.h"
#include "vdisk.h"
#include "video.h"
#include "fs.h"
#include "xroar.h"
#include "keyboard.h"
#include "hexs19.h"

static int init(void);
static void shutdown(void);
static void menu(void);
static char *get_filename(const char **extensions);

UIModule ui_gp32_module = {
	NULL,
	"gp32",
	"GP32 user-interface",
	init, shutdown,
	menu, get_filename, get_filename
};

#define FG (0xffffff00)
#define BG (0x00200000)

static void tape_callback(int);
static void machine_callback(int);
static void keymap_callback(int);
static void dragondos_callback(int);
static void artifact_callback(int);
static void snapshot_load_callback(int);
static void snapshot_save_callback(int);
static void binary_load_callback(int);
static void disk_callback(int);
static void reset_callback(int);

typedef struct Menu Menu;

typedef enum { MENU_SELECT, MENU_PICKLIST, MENU_PICKINT, MENU_SUBMENU } Menu_t;

typedef struct {
	const char *label;
	int *var;
	void (*callback)(int opt);
	unsigned int cur;
	unsigned int numopt;
	struct {
		const char *label;
		int value;
	} options[];
} MenuPickint;

typedef struct {
	const char *label;
	void (*callback)(int opt);
	int numopt;
	char *options[];
} MenuPicklist;

typedef struct {
	const char *label;
	void (*callback)(int num);
} MenuSelect;

typedef struct {
	Menu_t type;
	int hx, hw;  /* highlight x-offset and width */
	union {
		void *dummy;  /* gcc doesn't compare against all these
				 prototypes before warning. */
		MenuPickint *pickint;
		MenuSelect *select;
		MenuPicklist *picklist;
		Menu *submenu;
	} data;
} MenuOption;

struct Menu {
	const char *label;
	int numopt;
	MenuOption options[];
};

static MenuSelect snapshot_load_option = {
	"Load snapshot", snapshot_load_callback
};
static MenuSelect tape_attach_load_option = {
	"ATTACH AND LOAD FIRST PROGRAM", tape_callback
};
static MenuSelect tape_attach_option = {
	"ATTACH", tape_callback
};
static Menu tape_option = {
	"Cassette file",
	2,
	{ { MENU_SELECT, 0, 0, { &tape_attach_load_option } },
	  { MENU_SELECT, 0, 0, { &tape_attach_option } }
	}
};
static MenuSelect binary_load_option = {
	"Load binary", binary_load_callback
};
static MenuSelect snapshot_save_option = {
	"Save snapshot", snapshot_save_callback
};
static MenuPickint artifact_menuopt = {
	"Hi-res artifact mode",
	&video_artifact_mode, artifact_callback, 0,
	3, {
		{ "No artifacts", 0 },
		{ "Blue-red", 1 },
		{ "Red-blue", 2 },
	}
};
static MenuPickint machine_options = {
	"Emulated machine",
	&machine_romtype, machine_callback, 0,
	3, {
		{ "Dragon 64", DRAGON64 },
		{ "Dragon 32", DRAGON32 },
		{ "Tandy CoCo", COCO },
	}
};
static MenuPickint keymap_options = {
	"Keymap",
	&machine_keymap, keymap_callback, 0,
	2, {
		{ "Dragon", DRAGON_KEYBOARD },
		{ "Tandy CoCo", COCO_KEYBOARD },
	}
};
static MenuPickint dragondos_options = {
	"DragonDOS",
	&dragondos_enabled, dragondos_callback, 0,
	2, {
		{ "Disabled", 0 },
		{ "Enabled", 1 },
	}
};
static MenuPickint disk_options = {
	"Insert disk into drive",
	NULL, disk_callback, 0,
	4, {
		{ "1", 0 },
		{ "2", 1 },
		{ "3", 2 },
		{ "4", 3 },
	}
};
static MenuPickint reset_options = {
	"Reset machine",
	NULL, reset_callback, 0,
	2, {
		{ "Soft", RESET_SOFT },
		{ "Hard", RESET_HARD },
	}
};
static Menu main_menu = {
	"Main menu",
	10,
	{ { MENU_SELECT, 0, 0, { &snapshot_load_option } },
	  { MENU_SUBMENU, 0, 0, { &tape_option } },
	  { MENU_SELECT, 0, 0, { &binary_load_option } },
	  { MENU_SELECT, 0, 0, { &snapshot_save_option } },
	  { MENU_PICKINT, 0, 0, { &artifact_menuopt } },
	  { MENU_PICKINT, 0, 0, { &machine_options } },
	  { MENU_PICKINT, 0, 0, { &keymap_options } },
	  { MENU_PICKINT, 0, 0, { &dragondos_options } },
	  { MENU_PICKINT, 0, 0, { &disk_options } },
	  { MENU_PICKINT, 0, 0, { &reset_options } },
	}
};

extern GPDRAWSURFACE screen;
extern uint8_t vdg_alpha_gp32[2][3][8192];

static int init(void) {
	return 0;
}

static void shutdown(void) {
}

static void draw_char(int x, int y, char c) {
	uint32_t *dest = (uint32_t *)screen.ptbuffer + (59-(y*3)) + x*8*60;
	uint8_t *charset = (uint8_t *)vdg_alpha_gp32[1];
	uint32_t out;
	unsigned int i, j;
	c &= 0x7f;
	if (c >= 'a' && c <= 'z') {
		c -= 32;
	}
	c <<= 2;
	for (j = 0; j < 3; j++) {
		for (i = 0 ; i < 8; i++) {
			out = *(charset + (c << 3)) << 24;
			out |= *(charset + ((c+1) << 3)) << 16;
			out |= *(charset + ((c+2) << 3)) << 8;
			out |= *(charset + ((c+3) << 3));
			*dest = out;
			dest += 60;
			charset++;
		}
		dest -= 481;
		charset += 8184;
	}
}

static void draw_string(int x, int y, const char *s, int w) {
	if (s == NULL)
		return;
	for (; *s && w; w--) {
		draw_char(x++,y,*(s++));
	}
}

static void highlight_line(unsigned int x, unsigned int y, unsigned int w) {
	uint32_t *dest = (uint32_t *)screen.ptbuffer + (57-(y*3)) + x*8*60;
	uint_least16_t i;
	for (i = 0; i < w*8; i++) {
		*(dest++) ^= ~0;
		*(dest++) ^= ~0;
		*(dest++) ^= ~0;
		dest += 57;
	}
}

static void notify_box(const char *msg) {
	int newkey;
	int x = (40 - strlen(msg)) / 2;
	video_module->backup();
	video_module->fillrect(0, 8*12, 320, 3*12, BG);
	draw_string(x, 9, msg, 40);
	do {
		gpkeypad_poll(NULL, &newkey, NULL);
	} while (!newkey);
	video_module->restore();
}

/* extract is either a function to pull a string out of data given an index
 * or NULL.  If NULL, data is treated as (char **) */

static int menu_oldselect(char *(*extract)(void *, int), void *data, int n, int x, int y, int w, int h, int is_filesel) {
	int newkey, rkey;
	int base = 0, cur = 0, oldbase, oldcur, i;
	int step = h / 2;
	char search[16];
	int search_len = 0, do_search = 0;
	unsigned char chatkey;
	int done = 0;
	if (n < 1) return -1;
	sound_silence();
	video_module->fillrect(x*8, y*12, w*8, h*12, BG);
	if (is_filesel) {
		search[0] = 0;
		video_module->fillrect(1*8, 17*12, 16*8, 2*12, BG);
		video_module->fillrect(1+8, 18*12, 8, 12, FG);
	}
	for (i = 0; i < h && (base+i) < n; i++) {
		char *line;
		if (extract) line = extract(data, base+i);
		else         line = ((char **)data)[base+i];
		draw_string(x, y+i, line, w);
	}
	highlight_line(x, y+(cur-base), w);
	SPEED_SLOW;
	do {
		gpkeypad_poll(NULL, &newkey, &rkey);
		oldcur = cur;
		oldbase = base;
		if (rkey & GPC_VK_UP) cur--;
		if (rkey & GPC_VK_DOWN) cur++;
		if (rkey & GPC_VK_FL) cur -= step;
		if (rkey & GPC_VK_FR) cur += step;
		if (cur < 0) cur = 0;
		if (cur >= n) cur = n - 1;
		chatkey = gpchatboard_scan() & 0x7f;
		switch (chatkey) {
		case 0: break;
		case '\r': done = 1; break;
		case 27: done = -1; break;
		case 8:
			if (search_len > 0) {
				search_len--;
				search[search_len] = 0;
				do_search = 1;
			}
			break;
		default:
			if (search_len < 12) {
				search[search_len++] = chatkey;
				search[search_len] = 0;
				do_search = 1;
			}
			break;
		}
		if (do_search) {
			int s;
			video_module->fillrect(1*8, 17*12, 16*8, 2*12, BG);
			draw_string(1, 18, search, 16);
			video_module->fillrect((1+search_len)*8,18*12,8,12,FG);
			do_search = 0;
			for (s = 0; s < n; s++) {
				if (!strncmp(extract(data, s), search, search_len)) {
					cur = s; break;
				}
			}
		}
		base = cur - (cur % h);
		if (cur != oldcur) {
			highlight_line(x, y+(oldcur-oldbase), w);
			if (base != oldbase) {
				video_module->fillrect(x*8, y*12, w*8, h*12, BG);
				for (i = 0; i < h && (base+i) < n; i++) {
					draw_string(x, y+i, extract(data, base+i), w);
				}
			}
			highlight_line(x, y+(cur-base), w);
		}
		if (newkey & GPC_VK_START)
			done = -1;
		if (newkey & (GPC_VK_SELECT|GPC_VK_FA|GPC_VK_FB))
			done = 1;
	} while (!done);
	SPEED_FAST;
	if (done == -1)
		return -1;
	return cur;
}

static char *dirent_as_string(void *data, int n) {
	struct dirent **dir = (struct dirent **)data;
	static char result[18];
	result[0] = 0;
	if (dir && dir[n]) {
		if (dir[n]->attr & FS_ATTR_DIRECTORY) {
			strcpy(result, "<");
			strcat(result, dir[n]->d_name);
			strcat(result, ">");
		} else {
			strcpy(result, dir[n]->d_name);
		}
	}
	return result;
}

static const char **valid_exts;

static int valid_extension(struct dirent *ent) {
	const char **e = valid_exts;
	char *entext;
	if (!e)
		return 1;
	/* List all directories */
	if ((ent->attr & 16) && strcmp(ent->d_name, "."))
		return 1;
	entext = strrchr(ent->d_name, '.');
	if (!entext)
		return 0;
	entext++;
	while (*e) {
		if (!strcmp(entext, *e))
			return 1;
		e++;
	}
	return 0;
}

static int dirsort(const void *a, const void *b) {
	const struct dirent *aa = *(const struct dirent **)a;
	const struct dirent *bb = *(const struct dirent **)b;
	if ((aa->attr & FS_ATTR_DIRECTORY) && !(bb->attr & FS_ATTR_DIRECTORY))
		return -1;
	if (!(aa->attr & FS_ATTR_DIRECTORY) && (bb->attr & FS_ATTR_DIRECTORY))
		return 1;
	return strncmp(aa->d_name, bb->d_name, 16);
}

static char *get_filename(const char **extensions) {
	int num, sel;
	static char result[16];
	struct dirent **dir;
	valid_exts = extensions;
	num = fs_scandir(".", &dir, valid_extension, dirsort);
	if (num < 1) {
		fs_freedir(&dir);
		return NULL;
	}
	while (1) {
		video_module->backup();
		sel = menu_oldselect(dirent_as_string, dir, num, 1,1,16,16, 1);
		video_module->restore();
		if (sel == -1) {
			fs_freedir(&dir);
			return NULL;
		}
		if (!(dir[sel]->attr & FS_ATTR_DIRECTORY)) {
			memcpy(result, dir[sel]->d_name, 16);
			fs_freedir(&dir);
			return result;
		}
		fs_chdir(dir[sel]->d_name);
		fs_freedir(&dir);
		num = fs_scandir(".", &dir, valid_extension, dirsort);
	}
}

static void menu_show(Menu *m, int x, int y, int w, int h);
static void menu(void) {
	sound_silence();
	video_module->backup();
	SPEED_SLOW;
	menu_show(&main_menu, 1, 1, 38, 18);
	SPEED_FAST;
	video_module->restore();
}

static void show_option(int x, int y, int w, MenuOption *opt) {
	if (opt->type == MENU_PICKINT) {
		MenuPickint *data = opt->data.pickint;
		unsigned int i, x2 = x + strlen(data->label) + 1, w2 = 0;
		data->cur = 0;
		for (i = 0; i < data->numopt; i++) {
			if (data->var && *(data->var) == data->options[i].value)
				data->cur = i;
			if (strlen(data->options[i].label) > w2)
				w2 = strlen(data->options[i].label);
		}
		opt->hx = x2;
		opt->hw = w2;
		draw_string(x, y, data->label, w);
		draw_string(x2, y, data->options[data->cur].label, w2);
	}
	if (opt->type == MENU_SELECT) {
		MenuSelect *data = opt->data.select;
		opt->hx = x;
		opt->hw = w;
		draw_string(x, y, data->label, w);
	}
	if (opt->type == MENU_PICKLIST) {
		MenuPicklist *data = opt->data.picklist;
		opt->hx = x;
		opt->hw = w;
		draw_string(x, y, data->label, w);
	}
	if (opt->type == MENU_SUBMENU) {
		Menu *data = opt->data.submenu;
		opt->hx = x;
		opt->hw = w;
		draw_string(x, y, data->label, w);
	}
}

static int edit_option(int x, int y, int w, MenuOption *opt, int num, int newkey, int rkey) {
	if (opt->type == MENU_PICKINT) {
		MenuPickint *data = opt->data.pickint;
		int x2 = opt->hx, w2 = opt->hw;
		if (rkey & GPC_VK_LEFT) {
			video_module->fillrect(x2*8, y*12, w2*8, 12, BG);
			if (data->cur == 0)
				data->cur = data->numopt-1;
			else
				data->cur--;
			draw_string(x2, y, data->options[data->cur].label, w2);
			highlight_line(x2, y, w2);
			return 0;
		}
		if (rkey & GPC_VK_RIGHT) {
			video_module->fillrect(x2*8, y*12, w2*8, 12, BG);
			data->cur++;
			if (data->cur >= data->numopt)
				data->cur = 0;
			draw_string(x2, y, data->options[data->cur].label, w2);
			highlight_line(x2, y, w2);
			return 0;
		}
		if (newkey & (GPC_VK_SELECT|GPC_VK_FA|GPC_VK_FB)) {
			if (data->callback)
				data->callback(data->options[data->cur].value);
			return 1;
		}
	}
	if (opt->type == MENU_SELECT) {
		MenuSelect *data = opt->data.select;
		if (newkey & (GPC_VK_SELECT|GPC_VK_FA|GPC_VK_FB)) {
			highlight_line(x, y, w);
			data->callback(num);
			return 1;
		}
	}
	if (opt->type == MENU_PICKLIST) {
		//MenuPicklist *data = opt->data.picklist;
		return 0;
	}
	if (opt->type == MENU_SUBMENU) {
		Menu *data = opt->data.submenu;
		if (newkey  & (GPC_VK_SELECT|GPC_VK_FA|GPC_VK_FB)) {
			int nx = x + 4, ny = y + 1, nw = w - 4, nh = 19 - ny;
			highlight_line(x, y, w);
			menu_show(data, nx, ny, nw, nh);
			return 1;
		}
	}
	return 0;
}

static void menu_show(Menu *m, int x, int y, int w, int h) {
	int newkey, rkey;
	int base = 0, cur = 0, oldbase = -1, oldcur = -1, i;
	int step = h / 2;
	int done = 0;
	if (m->numopt < 1) return;
	video_module->fillrect(x*8, y*12, w*8, h*12, BG);
	do {
		if (base != oldbase) {
			video_module->fillrect(x*8, y*12, w*8, h*12, BG);
			for (i = 0; i < h && (base+i) < m->numopt; i++) {
				show_option(x, y+i, w, &m->options[base+i]);
			}
			oldbase = base;
		}
		if (cur != oldcur) {
			highlight_line(m->options[cur].hx, y+(cur-base), m->options[cur].hw);
			oldcur = cur;
		}
		gpkeypad_poll(NULL, &newkey, &rkey);
		done = edit_option(x, y+(cur-base), w, &m->options[cur], cur, newkey, rkey);
		if (rkey & GPC_VK_UP) cur--;
		if (rkey & GPC_VK_DOWN) cur++;
		if (rkey & GPC_VK_FL) cur -= step;
		if (rkey & GPC_VK_FR) cur += step;
		if (cur < 0) cur = 0;
		if (cur >= m->numopt) cur = m->numopt - 1;
		if (cur != oldcur)
			highlight_line(m->options[oldcur].hx, y+(oldcur-base), m->options[oldcur].hw);
		base = cur - (cur % h);
		if (newkey & GPC_VK_START)
			done = 1;
	} while (!done);
}

static void tape_callback(int num) {
	const char *tape_exts[] = { "CAS", NULL };
	char *filename;
	video_module->restore();
	filename = get_filename(tape_exts);
	if (filename) {
		if (num == 0) {
			if (tape_autorun(filename) > 0) {
				notify_box("COULD NOT DETECT PROGRAM TYPE");
			}
		} else {
			tape_attach(filename);
		}
	}
}
static void machine_callback(int num) {
	if (num != machine_romtype) {
		machine_set_romtype(num);
		xroar_reset(RESET_HARD);
	}
}
static void keymap_callback(int num) {
	machine_set_keymap(num);
}
static void dragondos_callback(int num) {
	dragondos_enabled = num;
}
static void snapshot_load_callback(int num) {
	const char *snap_exts[] = { "SNA", NULL };
	char *filename;
	(void)num;  /* unused */
	video_module->restore();
	filename = get_filename(snap_exts);
	if (filename)
		read_snapshot(filename);
}
static void snapshot_save_callback(int num) {
	//char *snap_exts[] = { "SNA", NULL };
	//char *filename;
	(void)num;  /* unused */
	video_module->restore();
	notify_box("SNAPSHOT SAVING NOT YET SUPPORTED");
	// filename = save_filename(snap_exts);
}
static void binary_load_callback(int num) {
	const char *bin_exts[] = { "BIN", NULL };
	char *filename;
	(void)num;  /* unused */
	video_module->restore();
	if (machine_romtype != COCO)
		notify_box("WARNING: USUALLY FOR COCO BINARIES");
	filename = get_filename(bin_exts);
	if (filename)
		coco_bin_read(filename);
}
static void artifact_callback(int num) {
	video_artifact_mode = num;
	vdg_set_mode();
}
static void disk_callback(int num) {
	const char *disk_exts[] = { "DMK", "JVC", "VDK", "DSK", NULL };
	char *filename;
	video_module->restore();
	filename = get_filename(disk_exts);
	if (filename)
		vdisk_load(filename, num);
}
static void reset_callback(int mode) {
	xroar_reset(mode);
}

#endif  /* HAVE_GP33 */
