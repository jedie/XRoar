/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>
#include <gpgraphic.h>

#include "types.h"
#include "gp32/gp32.h"
#include "gp32/gpgfx.h"
#include "gp32/gpkeypad.h"
#include "gp32/gpchatboard.h"
#include "gp32/ao_gp32.h"
#include "cart.h"
#include "fs.h"
#include "hexs19.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "snapshot.h"
#include "tape.h"
#include "vdg.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);
void gp32_menu(void);
static char *get_filename(const char **extensions);

static Module *empty_module_list[] = { NULL };

extern VideoModule video_gp32_module;
static VideoModule *gp32_video_module_list[] = {
	&video_gp32_module,
	NULL
};

extern SoundModule sound_gp32_module;
static SoundModule *gp32_sound_module_list[] = {
	&sound_gp32_module,
	NULL
};

extern KeyboardModule keyboard_gp32_module;
static KeyboardModule *gp32_keyboard_module_list[] = {
	&keyboard_gp32_module,
	NULL
};

UIModule ui_gp32_module = {
	{ "gp32", "GP32 user-interface",
	  init, 0, shutdown },
	(FileReqModule **)empty_module_list,
	gp32_video_module_list,
	gp32_sound_module_list,
	gp32_keyboard_module_list,
	(JoystickModule **)empty_module_list
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
const char *keymap_opts[] = { "Auto", "Dragon", "Tandy" };
const char *ram_opts[] = { "Auto", "4K", "16K", "32K", "64K" };
const char *onoff_opts[] = { "Disabled", "Enabled" };
const char *reset_opts[] = { "Soft", "Hard" };

static void load_file_callback(unsigned int);
static void snapshot_save_callback(unsigned int);
static void tape_callback(unsigned int);
static void disk_callback(unsigned int);
static void cart_callback(unsigned int);
static void artifact_callback(unsigned int);
static void machine_callback(unsigned int);
static void keymap_callback(unsigned int);
static void ram_callback(unsigned int);
static void extbas_callback(unsigned int);
static void dos_callback(unsigned int);
static void reset_callback(unsigned int);
static void do_hard_reset(unsigned int);

static Menu main_menu[] = {
	{ "Load file...", NULL, 0, 0, NULL, load_file_callback, NULL },
	{ "Save snapshot...", NULL, 0, 0, NULL, snapshot_save_callback, NULL },
	{ "Insert tape...", NULL, 0, 2, tape_opts, tape_callback, NULL },
	{ "Insert disk...", NULL, 0, 4, disk_opts, disk_callback, NULL },
	{ "Cartridge:", NULL, 0, 2, cart_opts, cart_callback, NULL },
	{ "Hi-res artifacts", &running_config.cross_colour_phase, 0, 3, artifact_opts, NULL, artifact_callback },
	{ "Emulated machine", &requested_machine, 1, 5, machine_names, machine_callback, NULL },
	{ "Keyboard layout", &requested_config.keymap, 0, 3, keymap_opts, keymap_callback, NULL },
	{ "RAM", NULL, 4, 5, ram_opts, do_hard_reset, ram_callback },
	{ "Extended BASIC", NULL, 1, 2, onoff_opts, do_hard_reset, extbas_callback },
	{ "DOS", NULL, 1, 2, onoff_opts, do_hard_reset, dos_callback },
	{ "Reset", NULL, 0, 2, reset_opts, reset_callback, NULL },
	{ NULL, NULL, 0, 0, NULL, NULL, NULL }
};

extern uint8_t vdg_alpha_gp32[4][3][8192];

static int init(void) {
	gpgfx_init();
	gpgfx_fillrect(0, 0, 320, 196, 0x00000000);
	gpgfx_fillrect(0, 196, 320, 44, 0xffffff00);
	return 0;
}

static void shutdown(void) {
}

static void draw_char(int x, int y, char c) {
	/* gp_screen.ptbuffer always aligned correctly: */
	void *ptbuffer = gp_screen.ptbuffer;
	uint32_t *dest = (uint32_t *)ptbuffer + (59-(y*3)) + x*8*60;
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
	/* gp_screen.ptbuffer always aligned correctly: */
	void *ptbuffer = gp_screen.ptbuffer;
	uint32_t *dest = (uint32_t *)ptbuffer + (57-(y*3)) + x*8*60;
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

#define REPEAT_MASK (GPC_VK_UP | GPC_VK_DOWN)

static int show_menu(Menu *m, int x, int y, int w, int h) {
	unsigned int old_repeat_mask;
	int old_repeat_rate;
	int newkey;
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
	gpkeypad_get_repeat(&old_repeat_mask, &old_repeat_rate);
	gpkeypad_set_repeat(REPEAT_MASK, 225);
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
		gpkeypad_poll(NULL, &newkey, NULL);
		do {
			oldcur = cur;
			gpkeypad_poll(NULL, &newkey, NULL);
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
			if (newkey & GPC_VK_UP) cur--;
			if (newkey & GPC_VK_DOWN) cur++;
			if (newkey & GPC_VK_FL) cur -= step;
			if (newkey & GPC_VK_FR) cur += step;
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
	gpkeypad_set_repeat(old_repeat_mask, old_repeat_rate);
	gpgfx_restore();
	if ((selected != -1) && m[selected].select_callback)
		m[selected].select_callback((unsigned int)m[selected].cur_opt);
	return selected;
}

void gp32_menu(void) {
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

static void load_file_callback(unsigned int opt) {
	const char *exts[] = {
		"SNA", "SN0", "SN1", "SN2", "SN3", "SN4", "SN5", "SN6",
		"SN7", "SN8", "SN9",
		"CAS", "BIN", "HEX",
		"DMK", "JVC", "VDK", "DSK",
		"ROM",
		NULL };
	char *filename;
	(void)opt;  /* unused */
	filename = get_filename(exts);
	if (filename == NULL)
		return;
	set_save_basename(filename);
	xroar_load_file(filename, XROAR_AUTORUN_CAS | XROAR_AUTORUN_CART);
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
			if (tape_autorun(filename) < 0) {
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
	if (filename) {
		vdrive_eject_disk(opt);
		vdrive_insert_disk(opt, vdisk_load(filename));
	}
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

static void artifact_callback(unsigned int opt) {
	running_config.cross_colour_phase = opt;
	vdg_set_mode();
}

static void machine_callback(unsigned int opt) {
	if ((int)opt != requested_machine) {
		machine_clear_requested_config();
		requested_machine = opt;
		machine_reset(RESET_HARD);
	}
}

static void keymap_callback(unsigned int opt) {
	keyboard_set_keymap(opt - 1);
}

static void ram_callback(unsigned int opt) {
	int opt_trans[5] = { ANY_AUTO, 4, 16, 32, 64 };
	requested_config.ram = opt_trans[opt % 5];
}

static void extbas_callback(unsigned int opt) {
	noextbas = opt;
}

static void dos_callback(unsigned int opt) {
	requested_config.dos_type = opt ? ANY_AUTO : DOS_NONE;
}

static void reset_callback(unsigned int opt) {
	machine_reset(opt);
}

static void do_hard_reset(unsigned int opt) {
	(void)opt;
	machine_reset(RESET_HARD);
}
