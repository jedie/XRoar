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
#include "vdg.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"
#include "nds/ndsgfx.h"
#include "nds/ndsipc.h"
#include "nds/ndsui.h"
#include "nds/ndsui_button.h"
#include "nds/ndsui_filelist.h"
#include "nds/ndsui_keyboard.h"
#include "nds/ndsui_scrollbar.h"
#include "nds/ndsui_textbox.h"

static int init(void);
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
	{ "nds", "NDS user-interface",
	  init, 0, shutdown },
	NULL,  /* use default filereq module list */
	nds_video_module_list,
	nds_sound_module_list,
	NULL,  /* nds_keyboard_module_list, */
	NULL  /* use default joystick module list */
};

static int input_joysticks_swapped = 0;

static event_t poll_pen_event;
static void do_poll_pen(void);

static void show_main_input_screen(void);
static void show_machine_configuration_screen(void);
static void show_input_configuration_screen(void);
static void show_input_control_select_screen(void);
static void show_file_requester(const char **extensions);
static void show_joystick_screen(void);
static void file_requester_callback(void (*func)(char *));
static void update_joystick_swap_state_text(void);
static char *ic_construct_button_label(int i);

static int init(void) {
	powerOn(POWER_ALL_2D|POWER_SWAP_LCDS);
	chdir("/dragon");

	ndsgfx_init();
	ndsgfx_fillrect(0, 0, 256, 192, NDS_DARKPURPLE);

	event_init(&poll_pen_event);
	poll_pen_event.dispatch = do_poll_pen;
	poll_pen_event.at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
	event_queue(&UI_EVENT_LIST, &poll_pen_event);

	show_main_input_screen();

	return 0;
}

static void shutdown(void) {
}

/* NDS-specific UI commands supplementing those in input.h */
#define UI_SWAP_SCREENS   (256)

#define NUM_BUTTONS (12)
static struct {
	int command;
	int arg;
} control_mapping[NUM_BUTTONS] = {
	{ INPUT_JOY_RIGHT_FIRE, 0 },  /* A      -> Right firebutton */
	{ INPUT_KEY, 32 },            /* B      -> Space */
	{ INPUT_KEY, 13 },            /* SELECT -> Enter */
	{ UI_SWAP_SCREENS, 0 },       /* START  -> Swap screens */
	{ INPUT_JOY_RIGHT_X, 255 },   /* RIGHT  -> Right joystick right */
	{ INPUT_JOY_RIGHT_X, 0 },     /* LEFT   -> Right joystick right */
	{ INPUT_JOY_RIGHT_Y, 0 },     /* UP     -> Right joystick up */
	{ INPUT_JOY_RIGHT_Y, 255 },   /* DOWN   -> Right joystick down */
	{ INPUT_JOY_RIGHT_FIRE, 0 },  /* R      -> Right firebutton */
	{ INPUT_JOY_RIGHT_FIRE, 0 },  /* L      -> Right firebutton */
	{ INPUT_KEY, 0 },             /* X      -> Shift */
	{ INPUT_SWAP_JOYSTICKS, 0 },  /* Y      -> Swap joysticks */
};

static void ui_input_control_press(int command, int arg) {
	(void)arg;
	switch (command) {
		case UI_SWAP_SCREENS:
			REG_POWERCNT ^= POWER_SWAP_LCDS;
			if (REG_POWERCNT & POWER_SWAP_LCDS)
				show_main_input_screen();
			else
				show_joystick_screen();
			break;
		case INPUT_SWAP_JOYSTICKS:
			input_control_press(command, arg);
			input_joysticks_swapped = !input_joysticks_swapped;
			update_joystick_swap_state_text();
			break;
		default:
			input_control_press(command, arg);
			break;
	}
}

static void do_poll_pen(void) {
	unsigned int keyinput, new_keyinput, rel_keyinput;
	static unsigned int old_keyinput = 0;
	int px, py;
	int i;

	/* Check for newly pressed or released buttons */
	keyinput = ~((REG_KEYINPUT & 0x3ff) | (IPC->buttons << 10));
	new_keyinput = keyinput & ~old_keyinput;
	rel_keyinput = ~keyinput & old_keyinput;

	for (i = 0; i < NUM_BUTTONS; i++) {
		if (new_keyinput & (1 << i)) {
			ui_input_control_press(control_mapping[i].command, control_mapping[i].arg);
		}
		if (rel_keyinput & (1 << i)) {
			if (control_mapping[i].command < 256) {
				input_control_release(control_mapping[i].command, control_mapping[i].arg);
			}
		}
	}

	if (keyinput & (1 << 16)) {
		px = IPC->touchXpx;
		py = IPC->touchYpx;
		if (old_keyinput & (1 << 16)) {
			ndsui_pen_move(px, py);
		} else {
			ndsui_pen_down(px, py);
		}
	} else {
		ndsui_pen_up();
	}
	old_keyinput = keyinput;
	poll_pen_event.at_cycle += OSCILLATOR_RATE / 100;
	event_queue(&UI_EVENT_LIST, &poll_pen_event);
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
	ndsui_clear_component_list();

	if (mi_keyboard == NULL) {
		mi_keyboard = new_ndsui_keyboard(15, 113, 1);
		ndsui_keyboard_keypress_callback(mi_keyboard, mi_key_press);
		ndsui_keyboard_keyrelease_callback(mi_keyboard, mi_key_release);
		ndsui_keyboard_shift_callback(mi_keyboard, mi_shift_update);
	}
	ndsui_add_component(mi_keyboard);

	if (mi_load_button == NULL) {
		mi_load_button = new_ndsui_button(0, 0, -1, "Load...", 0);
		ndsui_button_release_callback(mi_load_button, mi_load_release);
	}
	ndsui_add_component(mi_load_button);

	if (mi_machine_button == NULL) {
		mi_machine_button = new_ndsui_button(0, 18, -1, "Machine configuration", 0);
		ndsui_button_release_callback(mi_machine_button, mi_machine_release);
	}
	ndsui_add_component(mi_machine_button);

	if (mi_input_button == NULL) {
		mi_input_button = new_ndsui_button(0, 36, -1, "Input configuration", 0);
		ndsui_button_release_callback(mi_input_button, mi_input_release);
	}
	ndsui_add_component(mi_input_button);

	if (mi_save_snapshot_button == NULL) {
		mi_save_snapshot_button = new_ndsui_button(0, 54, -1, "Save snapshot", 0);
		ndsui_button_release_callback(mi_save_snapshot_button, mi_save_snapshot_release);
	}
	ndsui_add_component(mi_save_snapshot_button);

	if (mi_joystick_swap_state == NULL) {
		mi_joystick_swap_state = new_ndsui_button(65, 86, -1, "J0: Right | J1: Left ", 0);
		ndsui_button_release_callback(mi_joystick_swap_state, mi_joystick_swap_state_release);
	}
	update_joystick_swap_state_text();
	ndsui_add_component(mi_joystick_swap_state);

	if (mi_reset_button == NULL) {
		mi_reset_button = new_ndsui_button(220, 0, -1, "Reset", 0);
		ndsui_button_release_callback(mi_reset_button, mi_reset_release);
	}
	ndsui_add_component(mi_reset_button);

	ndsui_show_all_components();
}

static void mi_key_press(int sym) {
	KEYBOARD_PRESS(sym);
	keyboard_column_update();
	keyboard_row_update();
}

static void mi_key_release(int sym) {
	KEYBOARD_RELEASE(sym);
	keyboard_column_update();
	keyboard_row_update();
}

static void mi_shift_update(int state) {
	if (state)
		KEYBOARD_PRESS(0);
	else
		KEYBOARD_RELEASE(0);
	keyboard_column_update();
	keyboard_row_update();
}

static void mi_load_release(int id) {
	(void)id;
	show_file_requester(NULL);
	file_requester_callback(mi_load_file);
}

static void mi_load_file(char *filename) {
	irqDisable(IRQ_VBLANK | IRQ_VCOUNT);
	xroar_load_file(filename, 1);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
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
	irqDisable(IRQ_VBLANK | IRQ_VCOUNT);
	write_snapshot(filename);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
}

static void update_joystick_swap_state_text(void) {
	ndsui_button_set_label(mi_joystick_swap_state, input_joysticks_swapped ? "J0: Left  | J1: Right" : "J0: Right | J1: Left ");
}

static void mi_joystick_swap_state_release(int id) {
	(void)id;
	input_control_press(INPUT_SWAP_JOYSTICKS, 0);
	input_joysticks_swapped = !input_joysticks_swapped;
	update_joystick_swap_state_text();
}

static void mi_reset_release(int id) {
	(void)id;
	machine_reset(RESET_HARD);
}

/**************************************************************************/

/* Machine configuration screen, setup and callbacks */

static const char *cross_colour_labels[] = {
	"No cross-colour",
	"Blue-red cross-colour",
	"Red-blue cross-colour"
};

static struct ndsui_component *mc_machine_button = NULL;
static struct ndsui_component *mc_dos_type_button = NULL;
static struct ndsui_component *mc_cross_colour_button = NULL;
static struct ndsui_component *mc_close_button = NULL;

static void mc_machine_release(int id);
static void mc_dos_type_release(int id);
static void mc_cross_colour_release(int id);
static void mc_close_release(int id);

static void mc_update_labels(void) {
	const char *dos_type_label = "Machine default";
	const char *cross_colour_label = cross_colour_labels[0];
	if (requested_config.dos_type >= 0) {
		dos_type_label = dos_type_names[requested_config.dos_type];
	}
	if (running_config.cross_colour_phase >= 0) {
		cross_colour_label = cross_colour_labels[running_config.cross_colour_phase];
	}
	ndsui_button_set_label(mc_machine_button, machine_names[requested_machine]);
	ndsui_button_set_label(mc_dos_type_button, dos_type_label);
	ndsui_button_set_label(mc_cross_colour_button, cross_colour_label);
}

static void show_machine_configuration_screen(void) {
	ndsui_clear_component_list();

	if (mc_machine_button == NULL) {
		mc_machine_button = new_ndsui_button(0, 0, 132, NULL, 0);
		ndsui_button_release_callback(mc_machine_button, mc_machine_release);
	}
	ndsui_add_component(mc_machine_button);

	if (mc_dos_type_button == NULL) {
		mc_dos_type_button = new_ndsui_button(0, 18, 132, NULL, 0);
		ndsui_button_release_callback(mc_dos_type_button, mc_dos_type_release);
	}
	ndsui_add_component(mc_dos_type_button);

	if (mc_cross_colour_button == NULL) {
		mc_cross_colour_button = new_ndsui_button(0, 36, 132, NULL, 0);
		ndsui_button_release_callback(mc_cross_colour_button, mc_cross_colour_release);
	}
	ndsui_add_component(mc_cross_colour_button);

	if (mc_close_button == NULL) {
		mc_close_button = new_ndsui_button(220, 87, -1, "Close", 0);
		ndsui_button_release_callback(mc_close_button, mc_close_release);
	}
	ndsui_add_component(mc_close_button);

	/* Borrow this from main input screen */
	ndsui_add_component(mi_reset_button);

	mc_update_labels();

	ndsui_show_all_components();
}

static void mc_machine_release(int id) {
	(void)id;
	requested_machine = (requested_machine + 1) % NUM_MACHINE_TYPES;
	machine_clear_requested_config();
	mc_update_labels();
}

static void mc_dos_type_release(int id) {
	(void)id;
	requested_config.dos_type++;
	if (requested_config.dos_type >= NUM_DOS_TYPES)
		requested_config.dos_type = ANY_AUTO;
	mc_update_labels();
}

static void mc_cross_colour_release(int id) {
	(void)id;
	running_config.cross_colour_phase++;
	running_config.cross_colour_phase %= NUM_CROSS_COLOUR_PHASES;
	vdg_set_mode();
	mc_update_labels();
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
} ic_button[NUM_BUTTONS] = {
	{ NULL, "A",      0,   52 },
	{ NULL, "B",      0,   65 },
	{ NULL, "Select", 128, 39 },
	{ NULL, "Start",  128, 26 },
	{ NULL, "Right",  0,   39 },
	{ NULL, "Left",   0,   26 },
	{ NULL, "Up",     0,   0 },
	{ NULL, "Down",   0,   13 },
	{ NULL, "R",      128, 13 },
	{ NULL, "L",      128, 0 },
	{ NULL, "X",      0,   78 },
	{ NULL, "Y",      0,   91 },
};

static int ic_selected_button = -1;
static struct ndsui_component *ic_close_button = NULL;

static void ic_button_release(int id);
static void ic_close_release(int id);

static void show_input_configuration_screen(void) {
	int i;
	ndsui_clear_component_list();

	for (i = 0; i < NUM_BUTTONS; i++) {
		if (ic_button[i].component == NULL) {
			ic_button[i].component = new_ndsui_button(ic_button[i].x, ic_button[i].y, -1, ic_construct_button_label(i), 0);
			ic_button[i].component->id = i;
			ndsui_button_release_callback(ic_button[i].component, ic_button_release);
		}
		ndsui_add_component(ic_button[i].component);
	}

	if (ic_close_button == NULL) {
		ic_close_button = new_ndsui_button(220, 87, -1, "Close", 0);
		ndsui_button_release_callback(ic_close_button, ic_close_release);
	}
	ndsui_add_component(ic_close_button);

	ndsui_show_all_components();
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

#define NUM_CONTROL_SELECTIONS (12)
static struct {
	struct ndsui_component *component;
	const char *label;
	int x, y;
	int command, arg;
} ics_button[NUM_CONTROL_SELECTIONS] = {
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
	ndsui_clear_component_list();

	for (i = 0; i < NUM_CONTROL_SELECTIONS; i++) {
		if (ics_button[i].component == NULL) {
			ics_button[i].component = new_ndsui_button(ics_button[i].x, ics_button[i].y, -1, ics_button[i].label, 0);
			ics_button[i].component->id = i;
			ndsui_button_release_callback(ics_button[i].component, ics_button_release);
		}
		ndsui_add_component(ics_button[i].component);
	}

	if (ics_keyboard == NULL) {
		ics_keyboard = new_ndsui_keyboard(15, 113, 0);
		ndsui_keyboard_keyrelease_callback(ics_keyboard, ics_key_release);
	}
	ndsui_keyboard_set_shift_state(ics_keyboard, 0);
	ndsui_add_component(ics_keyboard);

	if (ics_cancel_button == NULL) {
		ics_cancel_button = new_ndsui_button(214, 87, -1, "Cancel", 0);
		ndsui_button_release_callback(ics_cancel_button, ics_cancel_release);
	}
	ndsui_add_component(ics_cancel_button);

	ndsui_show_all_components();
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

/* Joystick touch screen, setup and callbacks */

static struct ndsui_component *js_screen = NULL;

static void js_pen(struct ndsui_component *self, int x, int y);

static void show_joystick_screen(void) {
	ndsui_clear_component_list();

	if (js_screen == NULL) {
		js_screen = ndsui_new_component(0, 0, 256, 192);
		js_screen->pen_down = js_pen;
		js_screen->pen_move = js_pen;
	}
	ndsui_add_component(js_screen);

	ndsui_show_all_components();
}

static void js_pen(struct ndsui_component *self, int x, int y) {
	(void)self;
	if (y >= 128) y |= 3;  /* bias y downwards a bit */
	input_control_press(INPUT_JOY_RIGHT_X, x);
	input_control_press(INPUT_JOY_RIGHT_Y, (y * 4) / 3);
}

/**************************************************************************/

/* File requester screen, setup and callbacks */

static struct ndsui_component *fr_scrollbar = NULL;
static struct ndsui_component *fr_filelist = NULL;
static struct ndsui_component *fr_filename_text = NULL;
static struct ndsui_component *fr_keyboard = NULL;
static struct ndsui_component *fr_ok_button = NULL;
static struct ndsui_component *fr_cancel_button = NULL;
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
	ndsui_clear_component_list();

	if (fr_scrollbar == NULL) {
		fr_scrollbar = new_ndsui_scrollbar(144, 0, 16, 99);
		ndsui_scrollbar_offset_callback(fr_scrollbar, fr_scrollbar_callback);
	}
	ndsui_add_component(fr_scrollbar);

	if (fr_filelist == NULL) {
		fr_filelist = new_ndsui_filelist(0, 0, 144, 99);
		ndsui_filelist_num_files_callback(fr_filelist, fr_num_files_callback);
		ndsui_filelist_visible_callback(fr_filelist, fr_visible_callback);
		ndsui_filelist_file_select_callback(fr_filelist, fr_file_select_callback);
	}
	ndsui_add_component(fr_filelist);

	if (fr_filename_text == NULL) {
		fr_filename_text = new_ndsui_textbox(0, 100, 160, 128);
	}
	ndsui_add_component(fr_filename_text);

	if (fr_keyboard == NULL) {
		fr_keyboard = new_ndsui_keyboard(15, 113, 1);
		ndsui_keyboard_keypress_callback(fr_keyboard, fr_key_press);
		ndsui_keyboard_shift_callback(fr_keyboard, fr_shift_update);
	}
	ndsui_keyboard_set_shift_state(fr_keyboard, 0);
	ndsui_add_component(fr_keyboard);

	if (fr_ok_button == NULL) {
		fr_ok_button = new_ndsui_button(238, 87, -1, "OK", 0);
		ndsui_button_release_callback(fr_ok_button, fr_ok_release);
	}
	ndsui_add_component(fr_ok_button);

	if (fr_cancel_button == NULL) {
		fr_cancel_button = new_ndsui_button(195, 87, -1, "Cancel", 0);
		ndsui_button_release_callback(fr_cancel_button, fr_cancel_release);
	}
	ndsui_add_component(fr_cancel_button);

	ndsui_show_all_components();
	ndsui_filelist_open(fr_filelist, ".", extensions);
	ndsui_filelist_search_string(fr_filelist, ndsui_textbox_get_text(fr_filename_text));
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
	ndsui_filelist_search_string(fr_filelist, ndsui_textbox_get_text(fr_filename_text));
}

static void fr_shift_update(int state) {
	fr_shift_state = state;
}

static void fr_ok_release(int id) {
	(void)id;
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
	if (fr_ok_callback != NULL)
		fr_ok_callback(ndsui_textbox_get_text(fr_filename_text));
}

static void fr_cancel_release(int id) {
	(void)id;
	ndsui_filelist_close(fr_filelist);
	show_main_input_screen();
}

/**************************************************************************/

/* Misc helper functions */

#define INPUTARG_NONE  (0)
#define INPUTARG_XAXIS (1)
#define INPUTARG_YAXIS (2)
#define INPUTARG_KEY   (3)

#define NUM_COMMAND_TYPES (9)
static struct {
	int command;
	const char *name;
	int arg_type;
} command_types[NUM_COMMAND_TYPES] = {
	{ INPUT_JOY_RIGHT_X,    "JoyR",         INPUTARG_XAXIS },
	{ INPUT_JOY_RIGHT_Y,    "JoyR",         INPUTARG_YAXIS },
	{ INPUT_JOY_LEFT_X,     "JoyL",         INPUTARG_XAXIS },
	{ INPUT_JOY_LEFT_Y,     "JoyL",         INPUTARG_YAXIS },
	{ INPUT_JOY_RIGHT_FIRE, "JoyR Fire",    INPUTARG_NONE },
	{ INPUT_JOY_LEFT_FIRE,  "JoyL Fire",    INPUTARG_NONE },
	{ INPUT_KEY,            "Key",          INPUTARG_KEY },
	{ INPUT_SWAP_JOYSTICKS, "Swap Joystks", INPUTARG_NONE },
	{ UI_SWAP_SCREENS,      "Swap Screens", INPUTARG_NONE }
};

static const char *key_names[] = {
	"Shift", "", "", "", "", "", "", "",
	"Left", "Right", "Down", "", "Clear", "Enter", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "Break", "", "", "", "",
	"Space"
};

static char label_buf[21];

static char *ic_construct_button_label(int i) {
	const char *button_label = ic_button[i].label;
	int command = control_mapping[i].command;
	int arg = control_mapping[i].arg;
	const char *command_name = "";
	int arg_type = INPUTARG_NONE;
	const char *arg_label;
	int j;
	for (j = 0; j < NUM_COMMAND_TYPES; j++) {
		if (command == command_types[j].command) {
			command_name = command_types[j].name;
			arg_type = command_types[j].arg_type;
			break;
		}
	}
	label_buf[0] = 0;
	switch (arg_type) {
		case INPUTARG_NONE:
			snprintf(label_buf, sizeof(label_buf), "%6s | %-12s", button_label, command_name);
			break;
		case INPUTARG_XAXIS:
			arg_label = (arg < 128) ? "Left" : "Right";
			snprintf(label_buf, sizeof(label_buf), "%6s | %-4s %-7s", button_label, command_name, arg_label);
			break;
		case INPUTARG_YAXIS:
			arg_label = (arg < 128) ? "Up" : "Down";
			snprintf(label_buf, sizeof(label_buf), "%6s | %-4s %-7s", button_label, command_name, arg_label);
			break;
		case INPUTARG_KEY:
			if (arg == 94) {
				snprintf(label_buf, sizeof(label_buf), "%6s | %-3s Up", button_label, command_name);
			} else if (arg > 0x20) {
				snprintf(label_buf, sizeof(label_buf), "%6s | %-3s %-8c", button_label, command_name, arg);
			} else {
				snprintf(label_buf, sizeof(label_buf), "%6s | %-3s %-8s", button_label, command_name, key_names[arg]);
			}
			break;
		default:
			break;
	}
	return label_buf;
}
