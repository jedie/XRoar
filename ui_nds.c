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

#include "types.h"
#include "events.h"
#include "hexs19.h"
#include "keyboard.h"
#include "input.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "snapshot.h"
#include "tape.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"
#include "nds/ndsgfx.h"
#include "nds/ndsui.h"
#include "nds/ndsui_button.h"
#include "nds/ndsui_filelist.h"
#include "nds/ndsui_keyboard.h"
#include "nds/ndsui_scrollbar.h"

static int init(int argc, char **argv);
static void shutdown(void);

extern VideoModule video_nds_module;
static VideoModule *nds_video_module_list[] = {
	&video_nds_module,
	NULL
};

UIModule ui_nds_module = {
	{ "nds", "SDL user-interface",
	  init, 0, shutdown, NULL },
	NULL,  /* use default filereq module list */
	nds_video_module_list,
	NULL,  /* use default sound module list */
	NULL,  /* nds_keyboard_module_list, */
	NULL  /* use default joystick module list */
};

static struct ndsui_component *component_list = NULL;
static struct ndsui_component *current_component = NULL;

static void add_component(struct ndsui_component *c);
static void draw_all_components(void);
static void clear_component_list(void);

static event_t *poll_pen_event;
static void do_poll_pen(void *context);

static void show_main_input_screen(void);
static void show_file_requester(const char **extensions);
static void file_requester_callback(void (*func)(char *));

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	powerSET(POWER_ALL_2D|POWER_SWAP_LCDS);
	if (!fatInitDefault()) {
		LOG_ERROR("fatInitDefault() failed.\n");
	}

	ndsgfx_init();
	ndsgfx_fillrect(0, 0, 256, 192, 0);
	ndsgfx_fillrect(0, 101, 256, 9, 0);
	component_list = current_component = NULL;

	poll_pen_event = event_new();
	poll_pen_event->dispatch = do_poll_pen;
	poll_pen_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(&xroar_ui_events, poll_pen_event);

	show_main_input_screen();

	return 0;
}

static void shutdown(void) {
}

static struct {
	int command;
	int arg;
} control_mapping[9] = {
	{ INPUT_JOY_RIGHT_FIRE, 0 },  /* A      -> Right firebutton */
	{ INPUT_JOY_LEFT_FIRE, 0 },   /* B      -> Left firebutton */
	{ INPUT_KEY, 13 },            /* SELECT -> Enter */
	{ INPUT_KEY, 27 },            /* START  -> Break */
	{ INPUT_JOY_RIGHT_X, 255 },   /* RIGHT  -> Right joystick right */
	{ INPUT_JOY_RIGHT_X, 0 },     /* LEFT   -> Right joystick right */
	{ INPUT_JOY_RIGHT_Y, 0 },     /* UP     -> Right joystick up */
	{ INPUT_JOY_RIGHT_Y, 255 },   /* DOWN   -> Right joystick down */
	{ INPUT_KEY, 0 },             /* R      -> Shift */
	/* { INPUT_KEY, 0 }, */             /* L      -> Shift */
};

static void do_poll_pen(void *context) {
	unsigned int keyinput, keyxy, new_keyinput, rel_keyinput;
	static unsigned int old_keyinput = 0, old_keyxy = 0;
	int px, py;
	int i;

	(void)context;

	/* Check for newly pressed or released buttons */
	keyinput = ~(REG_KEYINPUT);
	new_keyinput = keyinput & ~old_keyinput;
	rel_keyinput = ~keyinput & old_keyinput;
	old_keyinput = keyinput;
	for (i = 0; i < 9; i++) {
		if (new_keyinput & (1 << i)) {
			input_control_press(control_mapping[i].command, control_mapping[i].arg);
		}
		if (rel_keyinput & (1 << i)) {
			input_control_release(control_mapping[i].command, control_mapping[i].arg);
		}
	}

	/* Complete hack!  L swaps joysticks... */
	if (new_keyinput & (1 << 9)) {
		control_mapping[0].command ^= 2;
		control_mapping[1].command ^= 2;
		control_mapping[4].command ^= 2;
		control_mapping[5].command ^= 2;
		control_mapping[6].command ^= 2;
		control_mapping[7].command ^= 2;
	}

	while (IPC->mailBusy);

	keyxy = ~(IPC->buttons);
	if (keyxy & (1<<6)) {
		px = IPC->touchXpx;
		py = IPC->touchYpx;
		if (old_keyxy & (1<<6)) {
			if (current_component) {
				if (current_component->pen_move) {
					current_component->pen_move(current_component, px, py);
				}
			}
		} else {
			struct ndsui_component *c;
			for (c = component_list; c; c = c->next) {
				if (c->visible && px >= c->x && px < (c->x+c->w) && py >= c->y && py < (c->y+c->h)) {
					current_component = c;
				}
			}
			if (current_component) {
				if (current_component->pen_down) {
					current_component->pen_down(current_component, px, py);
				}
			} else {
				if (px > 0xf0 && py < 0x10) {
					machine_reset(RESET_HARD);
				}
			}
		}
	} else {
		if (current_component) {
			if (current_component->pen_up) {
				current_component->pen_up(current_component);
			}
			current_component = NULL;
		}
	}
	old_keyxy = keyxy;
	poll_pen_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(&xroar_ui_events, poll_pen_event);
}

static void add_component(struct ndsui_component *c) {
	if (c == NULL) return;
	/* c->next being set implies component is already on the list */
	if (c->next != NULL) return;
	c->next = component_list;
	component_list = c;
	ndsui_draw_component(c);
}

/* Draw all visible components */
static void draw_all_components(void) {
	struct ndsui_component *c;
	for (c = component_list; c; c = c->next) {
		ndsui_draw_component(c);
	}
}

/* Undraw all visible components and clear the component list */
static void clear_component_list(void) {
	struct ndsui_component **c = &component_list;
	while (*c) {
		struct ndsui_component **next = &((*c)->next);
		ndsui_undraw_component(*c);
		*c = NULL;
		c = next;
	}
}

/**************************************************************************/

/* Main emulator input screen, setup and callbacks */

static struct ndsui_component *mi_keyboard = NULL;
static struct ndsui_component *mi_load_button = NULL;

static void mi_key_press(int sym);
static void mi_key_release(int sym);
static void mi_shift_update(int shift);
static void mi_load_release(void);
static void mi_load_file(char *filename);

static void show_main_input_screen(void) {
	clear_component_list();

	if (mi_keyboard == NULL) {
		mi_keyboard = new_ndsui_keyboard(13, 110);
		ndsui_keyboard_keypress_callback(mi_keyboard, mi_key_press);
		ndsui_keyboard_keyrelease_callback(mi_keyboard, mi_key_release);
		ndsui_keyboard_shift_callback(mi_keyboard, mi_shift_update);
		ndsui_show_component(mi_keyboard);
	}
	add_component(mi_keyboard);

	if (mi_load_button == NULL) {
		mi_load_button = new_ndsui_button(0, 0, "Load...");
		ndsui_button_release_callback(mi_load_button, mi_load_release);
		ndsui_show_component(mi_load_button);
	}
	add_component(mi_load_button);

	draw_all_components();
}

static void mi_key_press(int sym) {
	KEYBOARD_PRESS(sym);
}

static void mi_key_release(int sym) {
	KEYBOARD_RELEASE(sym);
}

static void mi_shift_update(int state) {
	if (state)
		KEYBOARD_PRESS(0);
	else
		KEYBOARD_RELEASE(0);
}

static void mi_load_release(void) {
	show_file_requester(NULL);
	file_requester_callback(mi_load_file);
}

static void mi_load_file(char *filename) {
	int type = xroar_filetype_by_ext(filename);
	switch (type) {
		case FILETYPE_VDK: case FILETYPE_JVC:
		case FILETYPE_DMK:
			vdrive_eject_disk(0);
			vdrive_insert_disk(0, vdisk_load(filename));
			break;
		case FILETYPE_BIN:
			coco_bin_read(filename); break;
		case FILETYPE_HEX:
			intel_hex_read(filename); break;
		case FILETYPE_SNA:
			read_snapshot(filename); break;
		case FILETYPE_CAS: default:
			tape_open_reading(filename);
			break;
	}
}

/**************************************************************************/

/* File requester screen, setup and callbacks */

static struct ndsui_component *fr_scrollbar = NULL;
static struct ndsui_component *fr_filelist = NULL;
static struct ndsui_component *fr_keyboard = NULL;
static struct ndsui_component *fr_ok_button = NULL;
static struct ndsui_component *fr_cancel_button = NULL;
static char *fr_filename = NULL;
static void (*fr_ok_callback)(char *);

static void fr_num_files_callback(int num_files);
static void fr_visible_callback(int visible);
static void fr_scrollbar_callback(int offset);
static void fr_key_press(int sym);
static void fr_key_release(int sym);
static void fr_ok_release(void);
static void fr_cancel_release(void);

static void show_file_requester(const char **extensions) {
	clear_component_list();

	if (fr_scrollbar == NULL) {
		fr_scrollbar = new_ndsui_scrollbar(144, 0, 24, 99);
		ndsui_scrollbar_offset_callback(fr_scrollbar, fr_scrollbar_callback);
		ndsui_show_component(fr_scrollbar);
	}
	add_component(fr_scrollbar);

	if (fr_filelist == NULL) {
		fr_filelist = new_ndsui_filelist(0, 0, 144, 99);
		ndsui_filelist_num_files_callback(fr_filelist, fr_num_files_callback);
		ndsui_filelist_visible_callback(fr_filelist, fr_visible_callback);
		ndsui_show_component(fr_filelist);
	}
	add_component(fr_filelist);

	if (fr_keyboard == NULL) {
		fr_keyboard = new_ndsui_keyboard(13, 110);
		ndsui_keyboard_keypress_callback(fr_keyboard, fr_key_press);
		ndsui_keyboard_keyrelease_callback(fr_keyboard, fr_key_release);
		ndsui_show_component(fr_keyboard);
	}
	add_component(fr_keyboard);

	if (fr_ok_button == NULL) {
		fr_ok_button = new_ndsui_button(238, 87, "OK");
		ndsui_button_release_callback(fr_ok_button, fr_ok_release);
		ndsui_show_component(fr_ok_button);
	}
	add_component(fr_ok_button);

	if (fr_cancel_button == NULL) {
		fr_cancel_button = new_ndsui_button(195, 87, "Cancel");
		ndsui_button_release_callback(fr_cancel_button, fr_cancel_release);
		ndsui_show_component(fr_cancel_button);
	}
	add_component(fr_cancel_button);

	if (fr_filename != NULL) {
		free(fr_filename);
		fr_filename = NULL;
	}

	draw_all_components();
	ndsui_filelist_open(fr_filelist, ".", extensions);
}

static void file_requester_callback(void (*func)(char *)) {
	fr_ok_callback = func;
}

static void fr_num_files_callback(int num_files) {
	ndsui_scrollbar_set_total(fr_scrollbar, num_files);
}

static void fr_visible_callback(int visible) {
	ndsui_scrollbar_set_visible(fr_scrollbar, visible);
}

static void fr_scrollbar_callback(int offset) {
	ndsui_filelist_set_offset(fr_filelist, offset);
}

static void fr_key_press(int sym) {
	if (sym == 27)
		show_main_input_screen();
}

static void fr_key_release(int sym) {
	(void)sym;
}

static void fr_ok_release(void) {
	fr_filename = ndsui_filelist_filename(fr_filelist);
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
	if (fr_ok_callback != NULL)
		fr_ok_callback(fr_filename);
}

static void fr_cancel_release(void) {
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
}
