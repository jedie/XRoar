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
#include <unistd.h>

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
#include "nds/ndsui_textbox.h"

static int init(int argc, char **argv);
static void shutdown(void);

extern VideoModule video_nds_module;
static VideoModule *nds_video_module_list[] = {
	&video_nds_module,
	NULL
};

extern SoundModule sound_nds_module;
static SoundModule *nds_sound_module_list[] = {
	&sound_nds_module,
	NULL
};

UIModule ui_nds_module = {
	{ "nds", "SDL user-interface",
	  init, 0, shutdown, NULL },
	NULL,  /* use default filereq module list */
	nds_video_module_list,
	nds_sound_module_list,
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
static void show_machine_configuration_screen(void);
static void show_input_configuration_screen(void);
static void show_input_control_select_screen(void);
static void show_file_requester(const char **extensions);
static void file_requester_callback(void (*func)(char *));
static void update_joystick_swap_state_text(void);

static int init(int argc, char **argv) {
	(void)argc;
	(void)argv;
	powerSET(POWER_ALL_2D|POWER_SWAP_LCDS);
	if (!fatInitDefault()) {
		LOG_ERROR("fatInitDefault() failed.\n");
	}
	chdir("fat:/dragon");

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

#define UI_SWAP_SCREENS   (256)

static struct {
	int command;
	int arg;
} control_mapping[10] = {
	{ INPUT_JOY_RIGHT_FIRE, 0 },  /* A      -> Right firebutton */
	{ INPUT_JOY_LEFT_FIRE, 0 },   /* B      -> Left firebutton */
	{ INPUT_KEY, 13 },            /* SELECT -> Enter */
	{ UI_SWAP_SCREENS, 0 },       /* START  -> Swap screens */
	{ INPUT_JOY_RIGHT_X, 255 },   /* RIGHT  -> Right joystick right */
	{ INPUT_JOY_RIGHT_X, 0 },     /* LEFT   -> Right joystick right */
	{ INPUT_JOY_RIGHT_Y, 0 },     /* UP     -> Right joystick up */
	{ INPUT_JOY_RIGHT_Y, 255 },   /* DOWN   -> Right joystick down */
	{ INPUT_KEY, 0 },             /* R      -> Shift */
	{ INPUT_SWAP_JOYSTICKS, 0 },  /* L      -> Shift */
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
	for (i = 0; i < 10; i++) {
		if (new_keyinput & (1 << i)) {
			if (control_mapping[i].command < 256) {
				input_control_press(control_mapping[i].command, control_mapping[i].arg);
				if (control_mapping[i].command == INPUT_SWAP_JOYSTICKS) {
					update_joystick_swap_state_text();
				}
			} else {
				switch (control_mapping[i].command) {
				case UI_SWAP_SCREENS:
					REG_POWERCNT ^= POWER_SWAP_LCDS;
					break;
				}
			}
		}
		if (rel_keyinput & (1 << i)) {
			if (control_mapping[i].command < 256) {
				input_control_release(control_mapping[i].command, control_mapping[i].arg);
			}
		}
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
static struct ndsui_component *mi_machine_button = NULL;
static struct ndsui_component *mi_input_button = NULL;
static struct ndsui_component *mi_save_snapshot_button = NULL;
static struct ndsui_component *mi_joystick_swap_state = NULL;
static struct ndsui_component *mi_reset_button = NULL;

static void mi_key_press(int sym);
static void mi_key_release(int sym);
static void mi_shift_update(int shift);
static void mi_load_release(int id);
static void mi_load_file(char *filename);
static void mi_machine_release(int id);
static void mi_input_release(int id);
static void mi_save_snapshot_release(int id);
static void mi_save_snapshot(char *filename);
static void mi_joystick_swap_state_release(int id);
static void mi_reset_release(int id);

static void show_main_input_screen(void) {
	clear_component_list();

	if (mi_keyboard == NULL) {
		mi_keyboard = new_ndsui_keyboard(13, 110, 1);
		ndsui_keyboard_keypress_callback(mi_keyboard, mi_key_press);
		ndsui_keyboard_keyrelease_callback(mi_keyboard, mi_key_release);
		ndsui_keyboard_shift_callback(mi_keyboard, mi_shift_update);
		ndsui_show_component(mi_keyboard);
	}
	add_component(mi_keyboard);

	if (mi_load_button == NULL) {
		mi_load_button = new_ndsui_button(0, 0, "Load...", 0, 0);
		ndsui_button_release_callback(mi_load_button, mi_load_release);
		ndsui_show_component(mi_load_button);
	}
	add_component(mi_load_button);

	if (mi_machine_button == NULL) {
		mi_machine_button = new_ndsui_button(0, 18, "Machine configuration", 0, 0);
		ndsui_button_release_callback(mi_machine_button, mi_machine_release);
		ndsui_show_component(mi_machine_button);
	}
	add_component(mi_machine_button);

	if (mi_input_button == NULL) {
		mi_input_button = new_ndsui_button(0, 36, "Input configuration", 0, 0);
		ndsui_button_release_callback(mi_input_button, mi_input_release);
		ndsui_show_component(mi_input_button);
	}
	add_component(mi_input_button);

	if (mi_save_snapshot_button == NULL) {
		mi_save_snapshot_button = new_ndsui_button(0, 54, "Save snapshot", 0, 0);
		ndsui_button_release_callback(mi_save_snapshot_button, mi_save_snapshot_release);
		ndsui_show_component(mi_save_snapshot_button);
	}
	add_component(mi_save_snapshot_button);

	if (mi_joystick_swap_state == NULL) {
		mi_joystick_swap_state = new_ndsui_button(65, 86, "J0: Right | J1: Left ", 0, 0);
		ndsui_button_release_callback(mi_joystick_swap_state, mi_joystick_swap_state_release);
		ndsui_show_component(mi_joystick_swap_state);
	}
	update_joystick_swap_state_text();
	add_component(mi_joystick_swap_state);

	if (mi_reset_button == NULL) {
		mi_reset_button = new_ndsui_button(220, 0, "Reset", 0, 0);
		ndsui_button_release_callback(mi_reset_button, mi_reset_release);
		ndsui_show_component(mi_reset_button);
	}
	add_component(mi_reset_button);

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

static void mi_load_release(int id) {
	(void)id;
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

static void mi_machine_release(int id) {
	(void)id;
	show_machine_configuration_screen();
}

static void mi_input_release(int id) {
	(void)id;
	show_input_configuration_screen();
}

static void mi_save_snapshot_release(int id) {
	(void)id;
	show_file_requester(NULL);
	file_requester_callback(mi_save_snapshot);
}

static void mi_save_snapshot(char *filename) {
	write_snapshot(filename);
}

static void update_joystick_swap_state_text(void) {
	ndsui_button_set_label(mi_joystick_swap_state, input_joysticks_swapped ? "J0: Left  | J1: Right" : "J0: Right | J1: Left ");
}

static void mi_joystick_swap_state_release(int id) {
	(void)id;
	input_control_press(INPUT_SWAP_JOYSTICKS, 0);
	update_joystick_swap_state_text();
}

static void mi_reset_release(int id) {
	(void)id;
	machine_reset(RESET_HARD);
}

/**************************************************************************/

/* Machine configuration screen, setup and callbacks */

static struct ndsui_component *mc_machine_button = NULL;
static struct ndsui_component *mc_close_button = NULL;

static void mc_machine_release(int id);
static void mc_close_release(int id);

static void show_machine_configuration_screen(void) {
	int i;
	clear_component_list();

	if (mc_machine_button == NULL) {
		int name_len = 0, longest_name = 0;
		for (i = 0; i < NUM_MACHINE_TYPES; i++) {
			if (strlen(machine_names[i]) > name_len) {
				name_len = strlen(machine_names[i]);
				longest_name = i;
			}
		}
		mc_machine_button = new_ndsui_button(0, 0, machine_names[longest_name], 0, 0);
		ndsui_button_release_callback(mc_machine_button, mc_machine_release);
		ndsui_show_component(mc_machine_button);
	}
	ndsui_button_set_label(mc_machine_button, machine_names[requested_machine]);
	add_component(mc_machine_button);

	if (mc_close_button == NULL) {
		mc_close_button = new_ndsui_button(220, 87, "Close", 0, 0);
		ndsui_button_release_callback(mc_close_button, mc_close_release);
		ndsui_show_component(mc_close_button);
	}
	add_component(mc_close_button);

	/* Borrow this from main input screen */
	add_component(mi_reset_button);

	draw_all_components();
}

static void mc_machine_release(int id) {
	(void)id;
	requested_machine = (requested_machine + 1) % NUM_MACHINE_TYPES;
	ndsui_button_set_label(mc_machine_button, machine_names[requested_machine]);
	machine_clear_requested_config();
}

static void mc_close_release(int id) {
	(void)id;
	show_main_input_screen();
}

/**************************************************************************/

/* Input configuration screen, setup and callbacks */

static struct {
	struct ndsui_component *component;
	const char *label;
	int x, y;
} ic_button[10] = {
	{ NULL, "A",      0,   52 },
	{ NULL, "B",      0,   65 },
	{ NULL, "Select", 128, 39 },
	{ NULL, "Start",  128, 26 },
	{ NULL, "Right",  0,   39 },
	{ NULL, "Left",   0,   26 },
	{ NULL, "Up",     0,   0 },
	{ NULL, "Down",   0,   13 },
	{ NULL, "R",      128, 13 },
	{ NULL, "L",      128, 0 }
};

#define INPUTARG_NONE  (0)
#define INPUTARG_XAXIS (1)
#define INPUTARG_YAXIS (2)
#define INPUTARG_KEY   (3)
static struct {
	const char *name;
	int arg_type;
} command_types[] = {
	{ "JoyR",         INPUTARG_XAXIS },
	{ "JoyR",         INPUTARG_YAXIS },
	{ "JoyL",         INPUTARG_XAXIS },
	{ "JoyL",         INPUTARG_YAXIS },
	{ "JoyR Fire",    INPUTARG_NONE },
	{ "",             INPUTARG_NONE },
	{ "JoyL Fire",    INPUTARG_NONE },
	{ "",             INPUTARG_NONE },
	{ "Key",          INPUTARG_KEY },
	{ "",             INPUTARG_NONE },
	{ "Swap joystks", INPUTARG_NONE },
	{ "Swap screens", INPUTARG_NONE }
};

static const char *key_names[] = {
	"Shift", "", "", "", "", "", "", "",
	"Left", "Right", "Down", "", "Clear", "Enter", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "Break", "", "", "", "",
	"Space"
};

static int ic_selected_button = -1;
static struct ndsui_component *ic_close_button = NULL;

static void ic_button_release(int id);
static void ic_close_release(int id);

static char *ic_construct_button_label(int i) {
	static char buf[21];
	const char *button_label = ic_button[i].label;
	int command = control_mapping[i].command;
	int arg = control_mapping[i].arg;
	const char *command_name = command_types[command].name;
	const char *arg_label;
	int arg_type = command_types[command].arg_type;
	buf[0] = 0;
	switch (arg_type) {
		case INPUTARG_NONE:
			snprintf(buf, sizeof(buf), "%6s | %-12s", button_label, command_name);
			break;
		case INPUTARG_XAXIS:
			arg_label = (arg < 128) ? "Left" : "Right";
			snprintf(buf, sizeof(buf), "%6s | %-4s %-7s", button_label, command_name, arg_label);
			break;
		case INPUTARG_YAXIS:
			arg_label = (arg < 128) ? "Up" : "Down";
			snprintf(buf, sizeof(buf), "%6s | %-4s %-7s", button_label, command_name, arg_label);
			break;
		case INPUTARG_KEY:
			if (arg > 0x20) {
				snprintf(buf, sizeof(buf), "%6s | %-3s %-8c", button_label, command_name, arg);
			} else {
				snprintf(buf, sizeof(buf), "%6s | %-3s %-8s", button_label, command_name, key_names[arg]);
			}
			break;
		default:
			break;
	}
	return buf;
}

static void show_input_configuration_screen(void) {
	int i;
	clear_component_list();

	for (i = 0; i < 10; i++) {
		if (ic_button[i].component == NULL) {
			ic_button[i].component = new_ndsui_button(ic_button[i].x, ic_button[i].y, ic_construct_button_label(i), i, 0);
			ndsui_button_release_callback(ic_button[i].component, ic_button_release);
			ndsui_show_component(ic_button[i].component);
		}
		add_component(ic_button[i].component);
	}

	if (ic_close_button == NULL) {
		ic_close_button = new_ndsui_button(220, 87, "Close", 0, 0);
		ndsui_button_release_callback(ic_close_button, ic_close_release);
		ndsui_show_component(ic_close_button);
	}
	add_component(ic_close_button);

	draw_all_components();
}

static void ic_button_release(int id) {
	ic_selected_button = id;
	show_input_control_select_screen();
}

static void ic_close_release(int id) {
	(void)id;
	show_main_input_screen();
}

/**************************************************************************/

/* Input control select screen, setup and callbacks */

static struct {
	struct ndsui_component *component;
	const char *label;
	int x, y;
	int command, arg;
} ics_button[12] = {
	{ NULL, "JoyR Up",    0, 0,    INPUT_JOY_RIGHT_Y, 0 },
	{ NULL, "JoyR Down",  0, 13,   INPUT_JOY_RIGHT_Y, 255 },
	{ NULL, "JoyR Left",  0, 26,   INPUT_JOY_RIGHT_X, 0 },
	{ NULL, "JoyR Right", 0, 39,   INPUT_JOY_RIGHT_X, 255 },
	{ NULL, "JoyR Fire",  0, 52,   INPUT_JOY_RIGHT_FIRE, 0 },
	{ NULL, "JoyL Up",    128, 0,  INPUT_JOY_LEFT_Y, 0 },
	{ NULL, "JoyL Down",  128, 13, INPUT_JOY_LEFT_Y, 255 },
	{ NULL, "JoyL Left",  128, 26, INPUT_JOY_LEFT_X, 0 },
	{ NULL, "JoyL Right", 128, 39, INPUT_JOY_LEFT_X, 255 },
	{ NULL, "JoyL Fire",  128, 52, INPUT_JOY_LEFT_FIRE, 0 },
	{ NULL, "Swap Joysticks", 0,   78,   INPUT_SWAP_JOYSTICKS, 0 },
	{ NULL, "Swap Screens",   128, 78,   UI_SWAP_SCREENS, 0 },
};
static struct ndsui_component *ics_keyboard = NULL;
static struct ndsui_component *ics_cancel_button = NULL;

static void ics_button_release(int id);
static void ics_key_release(int sym);
static void ics_cancel_release(int id);

static void show_input_control_select_screen(void) {
	int i;
	clear_component_list();

	for (i = 0; i < 12; i++) {
		if (ics_button[i].component == NULL) {
			ics_button[i].component = new_ndsui_button(ics_button[i].x, ics_button[i].y, ics_button[i].label, i, 0);
			ndsui_button_release_callback(ics_button[i].component, ics_button_release);
			ndsui_show_component(ics_button[i].component);
		}
		add_component(ics_button[i].component);
	}

	if (ics_keyboard == NULL) {
		ics_keyboard = new_ndsui_keyboard(13, 110, 0);
		ndsui_keyboard_keyrelease_callback(ics_keyboard, ics_key_release);
		ndsui_show_component(ics_keyboard);
	}
	ndsui_keyboard_set_shift_state(ics_keyboard, 0);
	add_component(ics_keyboard);

	if (ics_cancel_button == NULL) {
		ics_cancel_button = new_ndsui_button(214, 87, "Cancel", 0, 0);
		ndsui_button_release_callback(ics_cancel_button, ics_cancel_release);
		ndsui_show_component(ics_cancel_button);
	}
	add_component(ics_cancel_button);

	draw_all_components();
}

static void ics_button_release(int id) {
	if (ic_selected_button >= 0) {
		control_mapping[ic_selected_button].command = ics_button[id].command;
		control_mapping[ic_selected_button].arg = ics_button[id].arg;
		ndsui_button_set_label(ic_button[ic_selected_button].component, ic_construct_button_label(ic_selected_button));
	}
	show_input_configuration_screen();
}

static void ics_key_release(int sym) {
	if (ic_selected_button >= 0) {
		control_mapping[ic_selected_button].command = INPUT_KEY;
		control_mapping[ic_selected_button].arg = sym;
		ndsui_button_set_label(ic_button[ic_selected_button].component, ic_construct_button_label(ic_selected_button));
	}
	show_input_configuration_screen();
}

static void ics_cancel_release(int id) {
	(void)id;
	show_input_configuration_screen();
}

/**************************************************************************/

/* File requester screen, setup and callbacks */

static struct ndsui_component *fr_scrollbar = NULL;
static struct ndsui_component *fr_filelist = NULL;
static struct ndsui_component *fr_filename_text = NULL;
static struct ndsui_component *fr_keyboard = NULL;
static struct ndsui_component *fr_ok_button = NULL;
static struct ndsui_component *fr_cancel_button = NULL;
static char *fr_filename = NULL;
static int fr_shift_state = 0;
static void (*fr_ok_callback)(char *);

static void fr_num_files_callback(int num_files);
static void fr_visible_callback(int visible);
static void fr_file_select_callback(const char *filename);
static void fr_scrollbar_callback(int offset);
static void fr_key_press(int sym);
static void fr_shift_update(int shift);
static void fr_ok_release(int id);
static void fr_cancel_release(int id);

static void show_file_requester(const char **extensions) {
	clear_component_list();

	if (fr_scrollbar == NULL) {
		fr_scrollbar = new_ndsui_scrollbar(144, 0, 16, 99);
		ndsui_scrollbar_offset_callback(fr_scrollbar, fr_scrollbar_callback);
		ndsui_show_component(fr_scrollbar);
	}
	add_component(fr_scrollbar);

	if (fr_filelist == NULL) {
		fr_filelist = new_ndsui_filelist(0, 0, 144, 99);
		ndsui_filelist_num_files_callback(fr_filelist, fr_num_files_callback);
		ndsui_filelist_visible_callback(fr_filelist, fr_visible_callback);
		ndsui_filelist_file_select_callback(fr_filelist, fr_file_select_callback);
		ndsui_show_component(fr_filelist);
	}
	add_component(fr_filelist);

	if (fr_filename_text == NULL) {
		fr_filename_text = new_ndsui_textbox(0, 100, 160, 128);
		ndsui_show_component(fr_filename_text);
	}
	add_component(fr_filename_text);

	if (fr_keyboard == NULL) {
		fr_keyboard = new_ndsui_keyboard(13, 110, 1);
		ndsui_keyboard_keypress_callback(fr_keyboard, fr_key_press);
		ndsui_keyboard_shift_callback(fr_keyboard, fr_shift_update);
		ndsui_show_component(fr_keyboard);
	}
	ndsui_keyboard_set_shift_state(fr_keyboard, 0);
	add_component(fr_keyboard);

	if (fr_ok_button == NULL) {
		fr_ok_button = new_ndsui_button(238, 87, "OK", 0, 0);
		ndsui_button_release_callback(fr_ok_button, fr_ok_release);
		ndsui_show_component(fr_ok_button);
	}
	add_component(fr_ok_button);

	if (fr_cancel_button == NULL) {
		fr_cancel_button = new_ndsui_button(195, 87, "Cancel", 0, 0);
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

static void fr_file_select_callback(const char *filename) {
	if (fr_filename) {
		free(fr_filename);
		fr_filename = NULL;
	}
	if (filename)
		fr_filename = strdup(filename);
	ndsui_textbox_set_text(fr_filename_text, filename);
}

static void fr_scrollbar_callback(int offset) {
	ndsui_filelist_set_offset(fr_filelist, offset);
}

static void fr_key_press(int sym) {
	if (fr_shift_state) {
		if (sym > 0x20 && sym < 0x40) {
			sym ^= 0x10;
		} else if (sym >= 0x40 && sym < 0x7f) {
			sym ^= 0x20;
		}
	}
	ndsui_textbox_type_char(fr_filename_text, sym);
	if (fr_filename) {
		free(fr_filename);
	}
	fr_filename = ndsui_textbox_get_text(fr_filename_text);
	ndsui_filelist_search_string(fr_filelist, fr_filename);
}

static void fr_shift_update(int state) {
	fr_shift_state = state;
}

static void fr_ok_release(int id) {
	(void)id;
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
	if (fr_ok_callback != NULL)
		fr_ok_callback(fr_filename);
}

static void fr_cancel_release(int id) {
	(void)id;
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
}
