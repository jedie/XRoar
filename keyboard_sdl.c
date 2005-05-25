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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "config.h"
#include "types.h"
#include "hexs19.h"
#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "pia.h"
#include "snapshot.h"
#include "tape.h"
#include "ui.h"
#include "video.h"
#include "wd2797.h"
#include "xroar.h"
#include "keyboard.h"

static void getargs(int argc, char **argv);
static int init(void);
static void shutdown(void);
static void poll(void);

KeyboardModule keyboard_sdl_module = {
	NULL,
	"sdl",
	"SDL keyboard driver",
	getargs, init, shutdown,
	poll
};

struct keymap {
	const char *name;
	unsigned int *raw;
};

#include "keyboard_sdl_mappings.c"

static unsigned int control = 0, shift = 0;

static uint_least16_t sdl_to_keymap[768];

#define HSHIFT (0x100)
#define HALTGR (0x200)
#define DSHIFT (0x100)
#define DUNSHIFT (0x200)

static uint_least16_t unicode_to_dragon[128] = {
	0,            0,            0,            0,
	0,            0,            0,            0,
	DUNSHIFT+8,   DUNSHIFT+9,   DUNSHIFT+10,  0,
	DUNSHIFT+12,  DUNSHIFT+13,  0,            0,
	0,            0,            0,            0,
	0,            0,            0,            0,
	0,            0,            0,            DUNSHIFT+27,
	0,            0,            0,            0,
	DUNSHIFT+' ', DSHIFT+'1',   DSHIFT+'2',   DSHIFT+'3',
	DSHIFT+'4',   DSHIFT+'5',   DSHIFT+'6',   DSHIFT+'7',
	DSHIFT+'8',   DSHIFT+'9',   DSHIFT+':',   DSHIFT+';',
	DUNSHIFT+',', DUNSHIFT+'-', DUNSHIFT+'.', DUNSHIFT+'/', 
	DUNSHIFT+'0', DUNSHIFT+'1', DUNSHIFT+'2', DUNSHIFT+'3', 
	DUNSHIFT+'4', DUNSHIFT+'5', DUNSHIFT+'6', DUNSHIFT+'7', 
	DUNSHIFT+'8', DUNSHIFT+'9', DUNSHIFT+':', DUNSHIFT+';', 
	DSHIFT+',',   DSHIFT+'-',   DSHIFT+'.',   DSHIFT+'/',
	DUNSHIFT+'@', DSHIFT+'a',   DSHIFT+'b',   DSHIFT+'c',
	DSHIFT+'d',   DSHIFT+'e',   DSHIFT+'f',   DSHIFT+'g',
	DSHIFT+'h',   DSHIFT+'i',   DSHIFT+'j',   DSHIFT+'k',
	DSHIFT+'l',   DSHIFT+'m',   DSHIFT+'n',   DSHIFT+'o',
	DSHIFT+'p',   DSHIFT+'q',   DSHIFT+'r',   DSHIFT+'s',
	DSHIFT+'t',   DSHIFT+'u',   DSHIFT+'v',   DSHIFT+'w',
	DSHIFT+'x',   DSHIFT+'y',   DSHIFT+'z',   DSHIFT+10,
	DSHIFT+12,    DSHIFT+9,     DUNSHIFT+'^', DSHIFT+'^',
	DUNSHIFT+12,  DUNSHIFT+'a', DUNSHIFT+'b', DUNSHIFT+'c',
	DUNSHIFT+'d', DUNSHIFT+'e', DUNSHIFT+'f', DUNSHIFT+'g',
	DUNSHIFT+'h', DUNSHIFT+'i', DUNSHIFT+'j', DUNSHIFT+'k',
	DUNSHIFT+'l', DUNSHIFT+'m', DUNSHIFT+'n', DUNSHIFT+'o',
	DUNSHIFT+'p', DUNSHIFT+'q', DUNSHIFT+'r', DUNSHIFT+'s',
	DUNSHIFT+'t', DUNSHIFT+'u', DUNSHIFT+'v', DUNSHIFT+'w',
	DUNSHIFT+'x', DUNSHIFT+'y', DUNSHIFT+'z', 0,
	0,            0,            DUNSHIFT+12,  DUNSHIFT+8
};

static char *keymap_option;
static unsigned int *selected_keymap;
static int translated_keymap;

static void map_keyboard(unsigned int *map) {
	int i;
	for (i = 0; i < 256; i++)
		sdl_to_keymap[i] = i & 0x7f;
	if (map == NULL)
		return;
	while (*map) {
		unsigned int sdlkey = *(map++);
		unsigned int dgnkey = *(map++);
		sdl_to_keymap[sdlkey & 0xff] = dgnkey & 0x7f;
	}
}

static void getargs(int argc, char **argv) {
	int i;
	keymap_option = NULL;
	for (i = 1; i < (argc-1); i++) {
		if (!strcmp(argv[i], "-keymap")) {
			i++;
			keymap_option = argv[i];
		}
	}
}

static int init(void) {
	int i;
	selected_keymap = NULL;
	for (i = 0; mappings[i].name; i++) {
		if (selected_keymap == NULL
				&& !strcmp("uk", mappings[i].name)) {
			selected_keymap = mappings[i].raw;
		}
		if (keymap_option && !strcmp(keymap_option, mappings[i].name)) {
			selected_keymap = mappings[i].raw;
			LOG_DEBUG(2,"\tSelecting '%s' keymap\n",keymap_option);
		}
	}
	map_keyboard(selected_keymap);
	translated_keymap = 0;
	SDL_EnableUNICODE(translated_keymap);
	return 0;
}

static void shutdown(void) {
}

static void keypress(SDL_keysym *keysym) {
	SDLKey sym = keysym->sym;
	if (sym == SDLK_LSHIFT || sym == SDLK_RSHIFT) {
		shift = 1;
		KEYBOARD_PRESS(0);
		return;
	}
	if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) { control = 1; return; }
	if (control) {
		switch (sym) {
		case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
			{
			const char *disk_exts[] = { "VDK", NULL };
			char *filename = ui_module->load_filename(disk_exts);
			if (filename)
				wd2797_load_disk(filename, sym - SDLK_1);
			}
			break;
		case SDLK_a:
			video_artifact_mode++;
			if (video_artifact_mode > 2)
				video_artifact_mode = 0;
			vdg_set_mode();
			break;
		case SDLK_b:
			{
			const char *bin_exts[] = { "BIN", NULL };
			char *filename = ui_module->load_filename(bin_exts);
			if (filename)
				coco_bin_read(filename);
			}
			break;
		case SDLK_c:
			exit(0);
			break;
		case SDLK_e:
			dragondos_enabled = !dragondos_enabled;
			break;
		case SDLK_h:
			{
			const char *hex_exts[] = { "HEX", NULL };
			char *filename = ui_module->load_filename(hex_exts);
			if (filename)
				intel_hex_read(filename);
			}
			break;
			/*
		case SDLK_j:
			{
			joystick_t *tmp = sdl_joystick_right;
			sdl_joystick_right = sdl_joystick_left;
			sdl_joystick_left = tmp;
			}
			break;
			*/
		case SDLK_k:
			machine_set_keymap(machine_keymap+1);
			break;
		case SDLK_l:
			{
			const char *snap_exts[] = { "SNA", NULL };
			char *filename = ui_module->load_filename(snap_exts);
			if (filename)
				read_snapshot(filename);
			}
			break;
		case SDLK_m:
			machine_set_keymap(machine_keymap+1);
			machine_set_romtype(machine_romtype+1);
			xroar_reset(RESET_HARD);
			break;
			/*
		case SDLK_n:
			video_next();
			break;
			*/
		case SDLK_r:
			xroar_reset(shift ? RESET_HARD : RESET_SOFT);
			break;
		case SDLK_s:
			{
			const char *snap_exts[] = { "SNA", NULL };
			char *filename = ui_module->save_filename(snap_exts);
			if (filename)
				write_snapshot(filename);
			}
			break;
		case SDLK_t:
			{
			const char *tape_exts[] = { "CAS", NULL };
			char *filename = ui_module->load_filename(tape_exts);
			if (filename) {
				if (shift)
					tape_autorun(filename);
				else
					tape_attach(filename);
			}
			break;
			}
		case SDLK_z: // running out of letters...
			translated_keymap = !translated_keymap;
			/* UNICODE translation only used in
			 * translation mode */
			SDL_EnableUNICODE(translated_keymap);
			break;
		default:
			break;
		}
		return;
	}
	if (sym == SDLK_UP) { KEYBOARD_PRESS(94); return; }
	if (sym == SDLK_DOWN) { KEYBOARD_PRESS(10); return; }
	if (sym == SDLK_LEFT) { KEYBOARD_PRESS(8); return; }
	if (sym == SDLK_RIGHT) { KEYBOARD_PRESS(9); return; }
	if (sym == SDLK_HOME) { KEYBOARD_PRESS(12); return; }
	if (translated_keymap) {
		if (keysym->unicode == '\\') {
			/* CoCo and Dragon 64 in 64K mode have a different way
			 * of scanning for '\' */
			if (IS_COCO_KEYBOARD || (IS_DRAGON64 && !(PIA_1B.port_output & 0x04)))
				keyboard_queue(0x100+12);
			else
				keyboard_queue(0x300+'/');
			return;
		}
		if ((keysym->unicode == 8 || keysym->unicode == 127) && shift) {
			keyboard_queue(DSHIFT+8);
			return;
		}
		if (keysym->unicode == 163) {
			keyboard_queue(DSHIFT+'3');
			return;
		}
		if (keysym->unicode < 128)
			keyboard_queue(unicode_to_dragon[keysym->unicode]);
		return;
	}
	if (sym < 256) {
		unsigned int mapped = sdl_to_keymap[sym];
		KEYBOARD_PRESS(mapped);
	}
}

static void keyrelease(SDL_keysym *keysym) {
	SDLKey sym = keysym->sym;
	switch (sym) {
	case 0: break;
	case SDLK_LCTRL: case SDLK_RCTRL:
		control = 0; break;
	case SDLK_LSHIFT: case SDLK_RSHIFT:
		shift = 0;
		KEYBOARD_RELEASE(0); break;
	case SDLK_UP: KEYBOARD_RELEASE(94); break;
	case SDLK_DOWN: KEYBOARD_RELEASE(10); break;
	case SDLK_LEFT: KEYBOARD_RELEASE(8); break;
	case SDLK_RIGHT: KEYBOARD_RELEASE(9); break;
	case SDLK_HOME: KEYBOARD_RELEASE(12); break;
	default:
		if (sym < 256) {
			unsigned int mapped = sdl_to_keymap[sym];
			KEYBOARD_RELEASE(mapped);
		}
		break;
	}
}

static void poll(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
		switch(event.type) { 
			case SDL_VIDEORESIZE:
				if (video_module->resize) {
					video_module->resize(event.resize.w, event.resize.h);
				}
				break;
			case SDL_QUIT:
				exit(0); break;
			case SDL_KEYDOWN:
				keypress(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
			case SDL_KEYUP:
				keyrelease(&event.key.keysym);
				keyboard_column_update();
				keyboard_row_update();
				break;
			default:
				break;
		}
	}
}
