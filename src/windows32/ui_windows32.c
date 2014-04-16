/*

XRoar, Dragon and Tandy CoCo 1/2 emulator
Copyright 2003-2014 Ciaran Anscomb

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

*/

#include "config.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include "array.h"

#include "cart.h"
#include "events.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6847.h"
#include "module.h"
#include "sam.h"
#include "tape.h"
#include "vdisk.h"
#include "xroar.h"

#include "sdl/common.h"
#include "windows32/common_windows32.h"

#define TAG_TYPE_MASK (0x7f << 8)
#define TAG_VALUE_MASK (0xff)

#define TAG_SIMPLE_ACTION (1 << 8)

#define TAG_MACHINE (2 << 8)

#define TAG_CARTRIDGE (3 << 8)

#define TAG_TAPE_FLAGS (4 << 8)

#define TAG_INSERT_DISK (5 << 8)
#define TAG_NEW_DISK (6 << 8)
#define TAG_WRITE_ENABLE (7 << 8)
#define TAG_WRITE_BACK (8 << 8)

#define TAG_FULLSCREEN (9 << 8)
#define TAG_VDG_INVERSE (16 << 8)
#define TAG_CROSS_COLOUR (10 << 8)

#define TAG_FAST_SOUND (11 << 8)

#define TAG_KEYMAP (12 << 8)
#define TAG_KBD_TRANSLATE (13 << 8)
#define TAG_JOY_RIGHT (14 << 8)
#define TAG_JOY_LEFT (15 << 8)

enum {
	TAG_QUIT,
	TAG_RESET_SOFT,
	TAG_RESET_HARD,
	TAG_FILE_LOAD,
	TAG_FILE_RUN,
	TAG_FILE_SAVE_SNAPSHOT,
	TAG_TAPE_INPUT,
	TAG_TAPE_OUTPUT,
	TAG_TAPE_INPUT_REWIND,
	TAG_ZOOM_IN,
	TAG_ZOOM_OUT,
	TAG_JOY_SWAP,
};

static int num_machines = 0;
static int num_cartridges = 0;
static int is_kbd_translate = 0;
static _Bool is_fast_sound = 0;

static struct {
	const char *name;
	const char *description;
} joystick_names[] = {
	{ NULL, "None" },
	{ "joy0", "Joystick 0" },
	{ "joy1", "Joystick 1" },
	{ "kjoy0", "Keyboard" },
	{ "mjoy0", "Mouse" },
};
#define NUM_JOYSTICK_NAMES ARRAY_N_ELEMENTS(joystick_names)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static _Bool init(void);
static void ui_shutdown(void);
static void fullscreen_changed_cb(_Bool);
static void cross_colour_changed_cb(int);
static void vdg_inverse_cb(_Bool);
static void machine_changed_cb(int);
static void cart_changed_cb(int);
static void keymap_changed_cb(int);
static void joystick_changed_cb(int, const char *);
static void kbd_translate_changed_cb(_Bool);
static void fast_sound_changed_cb(_Bool);
static void update_tape_state(int);
static void update_drive_disk(int, struct vdisk *);
static void update_drive_write_enable(int, _Bool);
static void update_drive_write_back(int, _Bool);

/* Note: prefer the default order for sound and joystick modules, which
 * will include the SDL options. */

UIModule ui_windows32_module = {
	.common = { .name = "windows32", .description = "Windows SDL UI",
	            .init = init, .shutdown = ui_shutdown },
	.video_module_list = sdl_video_module_list,
	.keyboard_module_list = sdl_keyboard_module_list,
	.joystick_module_list = sdl_js_modlist,
	.run = sdl_run,
	.fullscreen_changed_cb = fullscreen_changed_cb,
	.cross_colour_changed_cb = cross_colour_changed_cb,
	.vdg_inverse_cb = vdg_inverse_cb,
	.machine_changed_cb = machine_changed_cb,
	.cart_changed_cb = cart_changed_cb,
	.keymap_changed_cb = keymap_changed_cb,
	.joystick_changed_cb = joystick_changed_cb,
	.kbd_translate_changed_cb = kbd_translate_changed_cb,
	.fast_sound_changed_cb = fast_sound_changed_cb,
	.update_tape_state = update_tape_state,
	.update_drive_disk = update_drive_disk,
	.update_drive_write_enable = update_drive_write_enable,
	.update_drive_write_back = update_drive_write_back,
};

static HMENU top_menu;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void setup_file_menu(void);
static void setup_view_menu(void);
static void setup_machine_menu(void);
static void setup_cartridge_menu(void);
static void setup_tool_menu(void);

static _Bool init(void) {
	if (!getenv("SDL_VIDEODRIVER"))
		putenv("SDL_VIDEODRIVER=windib");

	if (!SDL_WasInit(SDL_INIT_NOPARACHUTE)) {
		if (SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
			LOG_ERROR("Failed to initialise SDL: %s\n", SDL_GetError());
			return 0;
		}
	}
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_ERROR("Failed to initialise SDL video: %s\n", SDL_GetError());
		return 0;
	}

	SDL_version sdlver;
	SDL_SysWMinfo sdlinfo;
	SDL_VERSION(&sdlver);
	sdlinfo.version = sdlver;
	SDL_GetWMInfo(&sdlinfo);
	windows32_main_hwnd = sdlinfo.window;

	top_menu = CreateMenu();
	setup_file_menu();
	setup_view_menu();
	setup_machine_menu();
	setup_cartridge_menu();
	setup_tool_menu();

	return 1;
}

static void ui_shutdown(void) {
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void sdl_windows32_update_menu(_Bool fullscreen) {
	if (fullscreen) {
		SetMenu(windows32_main_hwnd, NULL);
		SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);
	} else {
		SetMenu(windows32_main_hwnd, top_menu);
		SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void setup_file_menu(void) {
	HMENU file_menu;
	HMENU submenu;

	file_menu = CreatePopupMenu();

	AppendMenu(file_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_FILE_RUN, "&Run...");
	AppendMenu(file_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_FILE_LOAD, "&Load...");

	AppendMenu(file_menu, MF_SEPARATOR, 0, NULL);

	submenu = CreatePopupMenu();
	AppendMenu(file_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Cassette");
	AppendMenu(submenu, MF_STRING, TAG_SIMPLE_ACTION | TAG_TAPE_INPUT, "Input Tape...");
	AppendMenu(submenu, MF_STRING, TAG_SIMPLE_ACTION | TAG_TAPE_OUTPUT, "Output Tape...");
	AppendMenu(submenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(submenu, MF_STRING, TAG_SIMPLE_ACTION | TAG_TAPE_INPUT_REWIND, "Rewind Input Tape");
	AppendMenu(submenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(submenu, MF_STRING, TAG_TAPE_FLAGS | TAPE_FAST, "Fast Loading");
	AppendMenu(submenu, MF_STRING, TAG_TAPE_FLAGS | TAPE_PAD, "Leader Padding");
	AppendMenu(submenu, MF_STRING, TAG_TAPE_FLAGS | TAPE_PAD_AUTO, "Automatic Padding");
	AppendMenu(submenu, MF_STRING, TAG_TAPE_FLAGS | TAPE_REWRITE, "Rewrite");

	AppendMenu(file_menu, MF_SEPARATOR, 0, NULL);

	for (int drive = 0; drive < 4; drive++) {
		char title[9];
		snprintf(title, sizeof(title), "Drive &%c", '1' + drive);
		submenu = CreatePopupMenu();
		AppendMenu(file_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, title);
		AppendMenu(submenu, MF_STRING, TAG_INSERT_DISK | drive, "Insert Disk...");
		AppendMenu(submenu, MF_STRING, TAG_NEW_DISK | drive, "New Disk...");
		AppendMenu(submenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(submenu, MF_STRING, TAG_WRITE_ENABLE | drive, "Write Enable");
		AppendMenu(submenu, MF_STRING, TAG_WRITE_BACK | drive, "Write Back");
	}

	AppendMenu(file_menu, MF_SEPARATOR, 0, NULL);
	AppendMenu(file_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_FILE_SAVE_SNAPSHOT, "&Save Snapshot...");
	AppendMenu(file_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_QUIT, "&Quit");

	AppendMenu(top_menu, MF_STRING | MF_POPUP, (uintptr_t)file_menu, "&File");
}

static void setup_view_menu(void) {
	HMENU view_menu;
	HMENU submenu;

	view_menu = CreatePopupMenu();

	submenu = CreatePopupMenu();
	AppendMenu(view_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Zoom");
	AppendMenu(submenu, MF_STRING, TAG_SIMPLE_ACTION | TAG_ZOOM_IN, "Zoom In");
	AppendMenu(submenu, MF_STRING, TAG_SIMPLE_ACTION | TAG_ZOOM_OUT, "Zoom Out");

	AppendMenu(view_menu, MF_SEPARATOR, 0, NULL);
	AppendMenu(view_menu, MF_STRING, TAG_FULLSCREEN, "Full Screen");
	AppendMenu(view_menu, MF_SEPARATOR, 0, NULL);
	AppendMenu(view_menu, MF_STRING, TAG_VDG_INVERSE, "Inverse Text");

	submenu = CreatePopupMenu();
	AppendMenu(view_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Cross-colour");
	for (int i = 0; xroar_cross_colour_list[i].name; i++) {
		AppendMenu(submenu, MF_STRING, TAG_CROSS_COLOUR | xroar_cross_colour_list[i].value, xroar_cross_colour_list[i].description);
	}

	AppendMenu(top_menu, MF_STRING | MF_POPUP, (uintptr_t)view_menu, "&View");
}

static void setup_machine_menu(void) {
	HMENU machine_menu;
	HMENU submenu;

	machine_menu = CreatePopupMenu();
	num_machines = machine_config_count();
	for (int i = 0; i < num_machines; i++) {
		struct machine_config *mc = machine_config_index(i);
		AppendMenu(machine_menu, MF_STRING, TAG_MACHINE | mc->index, mc->description);
	}

	AppendMenu(machine_menu, MF_SEPARATOR, 0, NULL);
	submenu = CreatePopupMenu();
	AppendMenu(machine_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Keyboard Map");
	AppendMenu(submenu, MF_STRING, TAG_KEYMAP | KEYMAP_DRAGON, "Dragon Layout");
	AppendMenu(submenu, MF_STRING, TAG_KEYMAP | KEYMAP_DRAGON200E, "Dragon 200-E Layout");
	AppendMenu(submenu, MF_STRING, TAG_KEYMAP | KEYMAP_COCO, "CoCo Layout");

	AppendMenu(machine_menu, MF_SEPARATOR, 0, NULL);
	submenu = CreatePopupMenu();
	AppendMenu(machine_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Right Joystick");
	for (int i = 0; i < NUM_JOYSTICK_NAMES; i++) {
		AppendMenu(submenu, MF_STRING, TAG_JOY_RIGHT | i, joystick_names[i].description);
	}
	submenu = CreatePopupMenu();
	AppendMenu(machine_menu, MF_STRING | MF_POPUP, (uintptr_t)submenu, "Left Joystick");
	for (int i = 0; i < NUM_JOYSTICK_NAMES; i++) {
		AppendMenu(submenu, MF_STRING, TAG_JOY_LEFT | i, joystick_names[i].description);
	}
	AppendMenu(machine_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_JOY_SWAP, "Swap Joysticks");

	AppendMenu(machine_menu, MF_SEPARATOR, 0, NULL);
	AppendMenu(machine_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_RESET_SOFT, "Soft Reset");
	AppendMenu(machine_menu, MF_STRING, TAG_SIMPLE_ACTION | TAG_RESET_HARD, "Hard Reset");

	AppendMenu(top_menu, MF_STRING | MF_POPUP, (uintptr_t)machine_menu, "&Machine");

	machine_changed_cb(xroar_machine_config ? xroar_machine_config->index : 0);
}

static void setup_cartridge_menu(void) {
	HMENU cartridge_menu;
	HMENU submenu;

	cartridge_menu = CreatePopupMenu();
	AppendMenu(cartridge_menu, MF_STRING, TAG_CARTRIDGE, "None");
	num_cartridges = cart_config_count();
	for (int i = 0; i < num_cartridges; i++) {
		struct cart_config *cc = cart_config_index(i);
		AppendMenu(cartridge_menu, MF_STRING, TAG_CARTRIDGE | (cc->index + 1), cc->description);
	}

	AppendMenu(top_menu, MF_STRING | MF_POPUP, (uintptr_t)cartridge_menu, "&Cartridge");

	cart_changed_cb(xroar_cart ? xroar_cart->config->index : 0);
}

static void setup_tool_menu(void) {
	HMENU tool_menu;
	HMENU submenu;

	tool_menu = CreatePopupMenu();
	AppendMenu(tool_menu, MF_STRING, TAG_KBD_TRANSLATE, "Keyboard Translation");
	AppendMenu(tool_menu, MF_STRING, TAG_FAST_SOUND, "Fast Sound");

	AppendMenu(top_menu, MF_STRING | MF_POPUP, (uintptr_t)tool_menu, "&Tool");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void sdl_windows32_handle_syswmevent(void *data) {
	SDL_SysWMmsg *msg = data;
	int tag = LOWORD(msg->wParam);
	int tag_type = tag & TAG_TYPE_MASK;
	int tag_value = tag & TAG_VALUE_MASK;
	switch (msg->msg) {
	case WM_COMMAND:
		switch (tag_type) {

		/* Simple actions: */
		case TAG_SIMPLE_ACTION:
			switch (tag_value) {
			case TAG_QUIT:
				{
					SDL_Event event;
					event.type = SDL_QUIT;
					SDL_PushEvent(&event);
				}
				break;
			case TAG_RESET_SOFT:
				xroar_soft_reset();
				break;
			case TAG_RESET_HARD:
				xroar_hard_reset();
				break;
			case TAG_FILE_RUN:
				xroar_run_file(NULL);
				break;
			case TAG_FILE_LOAD:
				xroar_load_file(NULL);
				break;
			case TAG_FILE_SAVE_SNAPSHOT:
				xroar_save_snapshot();
				break;
			case TAG_TAPE_INPUT:
				xroar_select_tape_input();
				break;
			case TAG_TAPE_OUTPUT:
				xroar_select_tape_output();
				break;
			case TAG_TAPE_INPUT_REWIND:
				if (tape_input)
					tape_rewind(tape_input);
				break;
			case TAG_ZOOM_IN:
				sdl_zoom_in();
				break;
			case TAG_ZOOM_OUT:
				sdl_zoom_out();
				break;
			case TAG_JOY_SWAP:
				xroar_swap_joysticks(1);
				break;
			default:
				break;
			}
			break;

		/* Machines: */
		case TAG_MACHINE:
			xroar_set_machine(tag_value);
			break;

		/* Cartridges: */
		case TAG_CARTRIDGE:
			{
				struct cart_config *cc = cart_config_index(tag_value - 1);
				xroar_set_cart(cc ? cc->name : NULL);
			}
			break;

		/* Cassettes: */
		case TAG_TAPE_FLAGS:
			tape_select_state(tape_get_state() ^ tag_value);
			break;

		/* Disks: */
		case TAG_INSERT_DISK:
			xroar_insert_disk(tag_value);
			break;
		case TAG_NEW_DISK:
			xroar_new_disk(tag_value);
			break;
		case TAG_WRITE_ENABLE:
			xroar_set_write_enable(1, tag_value, XROAR_TOGGLE);
			break;
		case TAG_WRITE_BACK:
			xroar_set_write_back(1, tag_value, XROAR_TOGGLE);
			break;

		/* Video: */
		case TAG_FULLSCREEN:
			xroar_set_fullscreen(1, XROAR_TOGGLE);
			break;
		case TAG_CROSS_COLOUR:
			xroar_set_cross_colour(1, tag_value);
			break;
		case TAG_VDG_INVERSE:
			xroar_set_vdg_inverted_text(1, XROAR_TOGGLE);
			break;
		/* Audio: */
		case TAG_FAST_SOUND:
			machine_select_fast_sound(!xroar_cfg.fast_sound);
			break;

		/* Keyboard: */
		case TAG_KEYMAP:
			xroar_set_keymap(tag_value);
			break;
		case TAG_KBD_TRANSLATE:
			xroar_set_kbd_translate(1, XROAR_TOGGLE);
			break;

		/* Joysticks: */
		case TAG_JOY_RIGHT:
			xroar_set_joystick(1, 0, joystick_names[tag_value].name);
			break;
		case TAG_JOY_LEFT:
			xroar_set_joystick(1, 1, joystick_names[tag_value].name);
			break;

		default:
			break;
		}
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void fullscreen_changed_cb(_Bool fs) {
	CheckMenuItem(top_menu, TAG_FULLSCREEN, MF_BYCOMMAND | (fs ? MF_CHECKED : MF_UNCHECKED));
}

static void cross_colour_changed_cb(int cc) {
	CheckMenuRadioItem(top_menu, TAG_CROSS_COLOUR, TAG_CROSS_COLOUR | 2, TAG_CROSS_COLOUR | cc, MF_BYCOMMAND);
}

static void vdg_inverse_cb(_Bool i) {
	CheckMenuItem(top_menu, TAG_VDG_INVERSE, MF_BYCOMMAND | (i ? MF_CHECKED : MF_UNCHECKED));
}

static void machine_changed_cb(int machine_type) {
	CheckMenuRadioItem(top_menu, TAG_MACHINE, TAG_MACHINE | (num_machines - 1), TAG_MACHINE | machine_type, MF_BYCOMMAND);
}

static void cart_changed_cb(int cart_index) {
	cart_index++;
	CheckMenuRadioItem(top_menu, TAG_CARTRIDGE, TAG_CARTRIDGE | num_cartridges, TAG_CARTRIDGE | cart_index, MF_BYCOMMAND);
}

static void keymap_changed_cb(int map) {
	CheckMenuRadioItem(top_menu, TAG_KEYMAP, TAG_KEYMAP | (NUM_KEYMAPS - 1), TAG_KEYMAP | map, MF_BYCOMMAND);
}

static void joystick_changed_cb(int port, const char *name) {
	int tag_base = (port == 0) ? TAG_JOY_RIGHT : TAG_JOY_LEFT;
	int sel = 0;
	if (name) {
		for (int i = 1; i < NUM_JOYSTICK_NAMES; i++)
			if (0 == strcmp(name, joystick_names[i].name)) {
				sel = i;
				break;
			}
	}
	CheckMenuRadioItem(top_menu, tag_base, tag_base | (NUM_JOYSTICK_NAMES - 1), tag_base | sel, MF_BYCOMMAND);
}

static void kbd_translate_changed_cb(_Bool state) {
	CheckMenuItem(top_menu, TAG_KBD_TRANSLATE, MF_BYCOMMAND | (state ? MF_CHECKED : MF_UNCHECKED));
}

static void fast_sound_changed_cb(_Bool state) {
	CheckMenuItem(top_menu, TAG_FAST_SOUND, MF_BYCOMMAND | (state ? MF_CHECKED : MF_UNCHECKED));
}

static void update_tape_state(int flags) {
	for (int i = 0; i < 4; i++) {
		int f = flags & (1 << i);
		int tag = TAG_TAPE_FLAGS | (1 << i);
		CheckMenuItem(top_menu, tag, MF_BYCOMMAND | (f ? MF_CHECKED : MF_UNCHECKED));
	}
}

static void update_drive_disk(int drive, struct vdisk *disk) {
	_Bool we = 1, wb = 0;
	if (disk) {
		we = !disk->write_protect;
		wb = disk->write_back;
	}
	update_drive_write_enable(drive, we);
	update_drive_write_back(drive, wb);
}

static void update_drive_write_enable(int drive, _Bool state) {
	if (drive < 0 || drive > 3)
		return;
	CheckMenuItem(top_menu, TAG_WRITE_ENABLE | drive, MF_BYCOMMAND | (state ? MF_CHECKED : MF_UNCHECKED));
}

static void update_drive_write_back(int drive, _Bool state) {
	if (drive < 0 || drive > 3)
		return;
	CheckMenuItem(top_menu, TAG_WRITE_BACK | drive, MF_BYCOMMAND | (state ? MF_CHECKED : MF_UNCHECKED));
}
