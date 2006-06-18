/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>
#include <gpgraphic.h>

#include "types.h"
#include "gp32/gp32.h"
#include "gp32/gpgfx.h"
#include "gp32/gpkeypad.h"
#include "gp32/gpchatboard.h"
#include "ui.h"
#include "cart.h"
#include "machine.h"
#include "snapshot.h"
#include "sound_gp32.h"
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

typedef struct {
	const char *label;
	unsigned int *default_opt;
	unsigned int cur_opt;
	unsigned int num_opts;
	const char **options;
	void (*select_callback)(unsigned int opt);
	void (*modify_callback)(unsigned int opt);
} Menu;

extern int gp_desired_sample_rate;

const char *tape_opts[] = { "Autorun", "Attach only" };
const char *disk_opts[] = { "Drive 1", "Drive 2", "Drive 3", "Drive 4" };
const char *cart_opts[] = { "Insert...", "Remove" };
const char *artifact_opts[] = { "Off", "Blue-red", "Red-blue" };
const char *machine_opts[NUM_MACHINES];
const char *keymap_opts[] = { "Dragon", "Tandy" };
const char *ram_opts[] = { "4K", "16K", "32K", "64K" };
const char *onoff_opts[] = { "Disabled", "Enabled" };
const char *reset_opts[] = { "Soft", "Hard" };

static void snapshot_load_callback(unsigned int);
static void snapshot_save_callback(unsigned int);
static void tape_callback(unsigned int);
static void disk_callback(unsigned int);
static void cart_callback(unsigned int);
static void binhex_callback(unsigned int);
static void artifact_callback(unsigned int);
static void machine_callback(unsigned int);
static void keymap_callback(unsigned int);
static void ram_callback(unsigned int);
static void extbas_callback(unsigned int);
static void dos_callback(unsigned int);
static void reset_callback(unsigned int);
static void do_hard_reset(unsigned int);

static Menu main_menu[] = {
	{ "Load snapshot...", NULL, 0, 0, NULL, snapshot_load_callback, NULL },
	{ "Save snapshot...", NULL, 0, 0, NULL, snapshot_save_callback, NULL },
	{ "Insert tape...", NULL, 0, 2, tape_opts, tape_callback, NULL },
	{ "Insert disk...", NULL, 0, 4, disk_opts, disk_callback, NULL },
	{ "Cartridge:", NULL, 0, 2, cart_opts, cart_callback, NULL },
	{ "Insert binary/hex record...", NULL, 0, 0, NULL, binhex_callback, NULL },
	{ "Hi-res artifacts", &video_artifact_mode, 0, 3, artifact_opts, NULL, artifact_callback },
	{ "Emulated machine", &machine_romtype, 0, 4, machine_opts, machine_callback, NULL },
	{ "Keyboard layout", &machine_keymap, 0, 2, keymap_opts, keymap_callback, NULL },
	{ "RAM", NULL, 3, 4, ram_opts, do_hard_reset, ram_callback },
	{ "Extended BASIC", NULL, 1, 2, onoff_opts, do_hard_reset, extbas_callback },
	{ "DOS", NULL, 1, 2, onoff_opts, do_hard_reset, dos_callback },
	{ "Reset", NULL, 0, 2, reset_opts, reset_callback, NULL },
	{ NULL, NULL, 0, 0, NULL, NULL, NULL }
};

extern uint8_t vdg_alpha_gp32[4][3][8192];

static int init(void) {
	int i;
	for (i = 0; i < NUM_MACHINES; i++)
		machine_opts[i] = machines[i].description;
	return 0;
}

static void shutdown(void) {
}

static void draw_char(int x, int y, char c) {
	uint32_t *dest = (uint32_t *)gp_screen.ptbuffer + (59-(y*3)) + x*8*60;
	uint8_t *charset = (uint8_t *)vdg_alpha_gp32[1];
	uint32_t out;
	int i, j;
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
	uint32_t *dest = (uint32_t *)gp_screen.ptbuffer + (57-(y*3)) + x*8*60;
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
	gpgfx_fillrect(0, 8*12, 320, 3*12, BG);
	draw_string(x, 9, msg, 40);
	do {
		gpkeypad_poll(NULL, &newkey, NULL);
	} while (!newkey);
}

static int valid_extension(char *filename, const char **exts) {
	char *entext;
	if (!exts)
		return 1;
	/* List all directories */
	entext = strrchr(filename, '.');
	if (!entext)
		return 0;
	entext++;
	while (*exts) {
		if (!strcmp(entext, *exts))
			return 1;
		exts++;
	}
	return 0;
}

static int dirsort(const void *a, const void *b) {
	const Menu *aa = (const Menu *)a;
	const Menu *bb = (const Menu *)b;
	if ((aa->cur_opt & 16) && !(bb->cur_opt & 16))
		return -1;
	if (!(aa->cur_opt & 16) && (bb->cur_opt & 16))
		return 1;
	return strncmp(aa->label, bb->label, 16);
}

static void show_menu_line(Menu *m, int x, int y, int w) {
	int hx = x, hw = 0;
	draw_string(x, y, m->label, w);
	if (m->num_opts > 0) {
		m->cur_opt %= m->num_opts;
		hx = x + 24;
		hw = strlen(m->options[m->cur_opt]) + 2;
		if (hw > ((x + w) - hx))
			hw = (x + w) - hx;
		draw_char(hx, y, '[');
		draw_string(hx + 1, y, m->options[m->cur_opt], hw);
		draw_char(hx + hw - 1, y, ']');
	}
}

static void highlight_menu_line(Menu *m, int x, int y, int w) {
	int hx = x, hw = 0;
	gpgfx_fillrect(x*8, y*12, w*8, 12, BG);
	show_menu_line(m, x, y, w);
	if (m->num_opts > 0) {
		m->cur_opt %= m->num_opts;
		hx = x + 24;
		hw = strlen(m->options[m->cur_opt]) + 2;
	} else {
		hw = strlen(m->label);
	}
	if (hw > ((x + w) - hx))
		hw = (x + w) - hx;
	highlight_line(hx, y, hw);
}

static int show_menu(Menu *m, int x, int y, int w, int h) {
	int newkey, rkey;
	int nument;
	int base, cur = 0, oldbase = -1, oldcur;
	int step = h / 2;
	int done = 0, selected = -1;
	int i;
	for (nument = 0; m[nument].label != NULL; nument++) {
		if (m[nument].default_opt != NULL)
			m[nument].cur_opt = *(m[nument].default_opt);
	}
	if (nument == 0) return -1;
	gpgfx_backup();
	do {
		base = cur - (cur % h);
		if (base != oldbase) {
			oldbase = base;
			gpgfx_fillrect(x*8, y*12, w*8, h*12, BG);
			for (i = base; i < (base+h) && i < nument; i++) {
				show_menu_line(&m[i], x, y + (i - base), w);
			}
		}
		highlight_menu_line(&m[cur], x, y + (cur - base), w);
		SPEED_SLOW;
		gpkeypad_poll(NULL, &newkey, &rkey);
		do {
			oldcur = cur;
			gpkeypad_poll(NULL, &newkey, &rkey);
			if (m[cur].num_opts > 0) {
				unsigned int old_opt = m[cur].cur_opt;
				if (newkey & GPC_VK_LEFT) {
					if (m[cur].cur_opt == 0)
						m[cur].cur_opt = m[cur].num_opts - 1;
					else
						m[cur].cur_opt--;
				}
				if (newkey & GPC_VK_RIGHT) {
					m[cur].cur_opt++;
					m[cur].cur_opt %= m[cur].num_opts;
				}
				if (m[cur].cur_opt != old_opt) {
					if (m[cur].modify_callback)
						m[cur].modify_callback(m[cur].cur_opt);
					highlight_menu_line(&m[cur], x, y + (cur - base), w);
				}
			}
			if (rkey & GPC_VK_UP) cur--;
			if (rkey & GPC_VK_DOWN) cur++;
			if (rkey & GPC_VK_FL) cur -= step;
			if (rkey & GPC_VK_FR) cur += step;
			if (cur < 0) cur = 0;
			if (cur >= nument) cur = nument - 1;
			if (newkey & GPC_VK_START) {
				done = 1;
			}
			if (newkey & (GPC_VK_SELECT|GPC_VK_FA|GPC_VK_FB)) {
				selected = cur;
				done = 1;
			}
		} while (cur == oldcur && !done);
		SPEED_FAST;
		if (cur != oldcur)
			show_menu_line(&m[oldcur], x, y + (oldcur - base), w);
	} while (!done);
	gpgfx_restore();
	if ((selected != -1) && m[selected].select_callback)
		m[selected].select_callback((unsigned int)m[selected].cur_opt);
	return selected;
}

static void menu(void) {
	sound_silence();
	show_menu(main_menu, 1, 1, 38, 18);
}

/* File requester */

static char *get_filename(const char **extensions) {
	static char result[16];
	Menu *filemenu;
	char *namebuf;
	char cwd[1024];
	unsigned long p_num, dummy;
	GPDIRENTRY gpentry;
	GPFILEATTR gpentry_attr;
	int selected;
	unsigned int i, j;

	fs_chdir(".");
	do {
		fs_getcwd(cwd, sizeof(cwd));
		GpDirEnumNum(cwd, &p_num);
		filemenu = malloc((p_num+1) * sizeof(Menu));
		if (filemenu == NULL)
			return NULL;
		namebuf = malloc(p_num * 16);
		if (namebuf == NULL) {
			free(filemenu);
			return NULL;
		}
		for (i = 0, j = 0; i < p_num; i++) {
			GpDirEnumList(cwd, i, 1, &gpentry, &dummy);
			if (strcmp(gpentry.name, ".") == 0)
				continue;
			GpFileAttr(gpentry.name, &gpentry_attr);
			if ((gpentry_attr.attr & 16) || valid_extension(gpentry.name, extensions)) {
				memcpy(namebuf + j * 16, gpentry.name, 16);
				filemenu[j].label = namebuf + j * 16;
				filemenu[j].default_opt = NULL;
				filemenu[j].cur_opt = gpentry_attr.attr;
				filemenu[j].num_opts = 0;
				filemenu[j].select_callback = NULL;
				filemenu[j].modify_callback = NULL;
				j++;
			}
		}
		filemenu[j].label = NULL;
		qsort(filemenu, j, sizeof(Menu), dirsort);
		selected = show_menu(filemenu, 1, 1, 38, 18);
		if (selected >= 0) {
			if (filemenu[selected].cur_opt & 16) {
				fs_chdir(filemenu[selected].label);
				selected = 0;
			} else {
				memcpy(result, filemenu[selected].label, 16);
				selected = -2;
			}
		}
		free(namebuf);
		free(filemenu);
	} while (selected >= 0);
	if (selected == -2)
		return result;
	return NULL;
}

/* Menu callbacks */

static char *save_basename;

static void set_save_basename(char *source) {
	char *dot;
	int tocopy;
	if (save_basename) {
		free(save_basename);
		save_basename = NULL;
	}
	if (source == NULL)
		return;
	if ((dot = strchr(source, '.'))) {
		tocopy = dot - source;
	} else {
		tocopy = strlen(source);
	}
	if (tocopy > 8)
		tocopy = 8;
	save_basename = malloc(tocopy + 1);
	if (save_basename == NULL)
		return;
	strncpy(save_basename, source, tocopy);
	save_basename[tocopy] = 0;
}

static void snapshot_load_callback(unsigned int opt) {
	const char *snap_exts[] = { "SNA", "SN0", "SN1", "SN2", "SN3", "SN4",
		"SN5", "SN6", "SN7", "SN8", "SN9", NULL };
	char *filename;
	(void)opt;  /* unused */
	filename = get_filename(snap_exts);
	if (filename) {
		set_save_basename(filename);
		read_snapshot(filename);
	}
}

static void snapshot_save_callback(unsigned int opt) {
	const char *snap_exts[] = { "SN0", "SN1", "SN2", "SN3", "SN4",
		"SN5", "SN6", "SN7", "SN8", "SN9", NULL };
	int selected;
	Menu savemenu[2];
	char filename[13];
	(void)opt;  /* unused */
	if (save_basename == NULL) {
		notify_box("Unable to save snapshot");
		return;
	}
	memset(savemenu, 0, sizeof(savemenu));
	savemenu[0].label = save_basename;
	savemenu[0].num_opts = 10;
	savemenu[0].options = snap_exts;
	selected = show_menu(savemenu, 1, 9, 38, 1);
	if (selected < 0)
		return;
	strcpy(filename, save_basename);
	strcat(filename, ".");
	strcat(filename, snap_exts[selected]);
	write_snapshot(filename);
}

static void tape_callback(unsigned int opt) {
	const char *tape_exts[] = { "CAS", NULL };
	char *filename;
	filename = get_filename(tape_exts);
	if (filename) {
		set_save_basename(filename);
		if (opt == 0) {
			if (tape_autorun(filename) > 0) {
				notify_box("COULD NOT DETECT PROGRAM TYPE");
			}
		} else {
			tape_open_reading(filename);
		}
	}
}

static void disk_callback(unsigned int opt) {
	const char *disk_exts[] = { "DMK", "JVC", "VDK", "DSK", NULL };
	char *filename;
	filename = get_filename(disk_exts);
	if (filename)
		vdisk_load(filename, opt);
}

static void cart_callback(unsigned int opt) {
	const char *cart_exts[] = { "ROM", NULL };
	char *filename;
	if (opt == 0) {
		filename = get_filename(cart_exts);
		if (filename)
			cart_insert(filename, 1);
	} else {
		cart_remove();
	}
}

static void binhex_callback(unsigned int opt) {
	const char *exts[] = { "BIN", NULL };
	char *filename;
	(void)opt;
	filename = get_filename(exts);
	if (!IS_COCO)
		notify_box("Warning: Usually for CoCo binaries");
	if (filename) {
		set_save_basename(filename);
		coco_bin_read(filename);
	}
}

static void artifact_callback(unsigned int opt) {
	video_artifact_mode = opt;
	vdg_set_mode();
}

static void machine_callback(unsigned int opt) {
	if (opt != machine_romtype) {
		machine_set_romtype(opt);
		machine_reset(RESET_HARD);
	}
}

static void keymap_callback(unsigned int opt) {
	machine_set_keymap(opt);
}

static void ram_callback(unsigned int opt) {
	(void)opt;
	notify_box("Not yet implemented: Will always be 64K");
}

static void extbas_callback(unsigned int opt) {
	noextbas = opt;
}

static void dos_callback(unsigned int opt) {
	dragondos_enabled = opt;
}

static void reset_callback(unsigned int opt) {
	machine_reset(opt);
}

static void do_hard_reset(unsigned int opt) {
	(void)opt;
	machine_reset(RESET_HARD);
}
