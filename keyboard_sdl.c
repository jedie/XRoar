/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2004  Ciaran Anscomb
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

#include "config.h"

#ifdef HAVE_SDL_KEYBOARD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "types.h"
#include "keyboard.h"
#include "pia.h"
#include "snapshot.h"
#include "video.h"
#include "xroar.h"
#include "logging.h"
#include "ui.h"
#include "tape.h"
#include "wd2797.h"
#include "hexs19.h"

static int init(void);
static void shutdown(void);
static void poll(void);

KeyboardModule keyboard_sdl_module = {
	NULL,
	"sdl",
	"SDL keyboard driver",
	init, shutdown,
	poll
};

static uint_fast8_t control = 0, shift = 0;

/* Mostly identical positioning (deliberately so), but doing this lets
 * us be more clever and maybe queue shift+key in "translation mode" when
 * I implement that */
static uint_fast8_t sdl_to_keymap[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 0 - 7 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 8 - 15 */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 16 - 23 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 24 - 31 */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0xb7/**/, /* 32 - 39 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x3a, 0x2e, 0x2f, /* 40 - 47 */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 48 - 55 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x2d, 0x3e, 0x3f, /* 56 - 63 */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /* 64 - 71 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /* 72 - 79 */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /* 80 - 87 */
	0x58, 0x59, 0x5a, 0x40, 0x5c, 0x5d, 0x5e, 0x5f, /* 88 - 95 */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 96 - 103 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 104 - 111 */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 112 - 119 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 120 - 127 */
	/* Shifted set - only used in 'translation' mode */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0xb2/**/,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x40, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x40, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

static int init(void) {
	return 0;
}

static void shutdown(void) {
}

static void key_press(SDLKey sym) {
	switch (sym) {
	case SDLK_UP: KEYBOARD_PRESS(94); break;
	case SDLK_DOWN: KEYBOARD_PRESS(10); break;
	case SDLK_LEFT: KEYBOARD_PRESS(8); break;
	case SDLK_RIGHT: KEYBOARD_PRESS(9); break;
	case SDLK_LCTRL: case SDLK_RCTRL:
		control = 1; break;
	case SDLK_LSHIFT: case SDLK_RSHIFT:
		shift = 0x80;
		KEYBOARD_PRESS(0); break;
	default: if (sym == SDLK_c && control) exit(0);
		if (sym == SDLK_s && control) {
			char *snap_exts[] = { "SNA", NULL };
			char *filename = ui_module->get_filename(snap_exts);
			if (filename)
				write_snapshot(filename);
			break;
		}
		if (sym == SDLK_l && control) {
			char *snap_exts[] = { "SNA", NULL };
			char *filename = ui_module->get_filename(snap_exts);
			if (filename)
				read_snapshot(filename);
			break;
		}
		if (sym == SDLK_r && control) {
			xroar_reset(shift ? RESET_HARD : RESET_SOFT);
			break;
		}

		if (sym == SDLK_e && control) { dragondos_enabled = !dragondos_enabled; break; }
		if (sym == SDLK_d && control) {
			char *disk_exts[] = { "VDK", NULL };
			char *filename = ui_module->get_filename(disk_exts);
			if (filename)
				wd2797_load_disk(filename, 0);
			break;
		}
		if (sym == SDLK_m && control) {
			machine_set_keymap(machine_keymap+1);
			machine_set_romtype(machine_romtype+1);
			xroar_reset(RESET_HARD);
			break;
		}
		if (sym == SDLK_k && control) {
			machine_set_keymap(machine_keymap+1);
			break;
		}
#ifdef TRACE
		//if (sym == SDLK_t && control) { trace ^= 1; break; }
#endif
		if (sym == SDLK_t && control) {
			char *tape_exts[] = { "CAS", NULL };
			char *filename = ui_module->get_filename(tape_exts);
			if (filename) {
				if (shift)
					tape_autorun(filename);
				else
					tape_attach(filename);
			}
			break;
		}
		if (sym == SDLK_h && control) {
			char *hex_exts[] = { "HEX", NULL };
			char *filename = ui_module->get_filename(hex_exts);
			if (filename)
				intel_hex_read(filename);
			return;
		}
		if (sym == SDLK_b && control) {
			char *bin_exts[] = { "BIN", NULL };
			char *filename = ui_module->get_filename(bin_exts);
			if (filename)
				coco_bin_read(filename);
			return;
		}
		if (sym == SDLK_n && control) { video_next(); return; }
		if (sym == SDLK_a && control) { video_artifact_mode++; if (video_artifact_mode > 2) video_artifact_mode = 0; vdg_set_mode(); break; }
		if (sym < 128) {
			sym = sdl_to_keymap[sym|shift];
			if (sym & 0x80) {
				KEYBOARD_QUEUE(sym);
			} else {
				KEYBOARD_PRESS(sym);
			}
		}
		break;
	}
}

static void key_release(SDLKey sym) {
	switch (sym) {
	case SDLK_UP: KEYBOARD_RELEASE(94); break;
	case SDLK_DOWN: KEYBOARD_RELEASE(10); break;
	case SDLK_LEFT: KEYBOARD_RELEASE(8); break;
	case SDLK_RIGHT: KEYBOARD_RELEASE(9); break;
	case SDLK_LCTRL: case SDLK_RCTRL:
		control = 0; break;
	case SDLK_LSHIFT: case SDLK_RSHIFT:
		shift = 0;
		KEYBOARD_RELEASE(0); break;
	default:
		if (sym < 128) {
			/* Here we don't care about shifted keys - releasing
			 * a key that hasn't been pressed isn't a problem */
			sym = sdl_to_keymap[sym];
			if (!(sym & 0x80))
				KEYBOARD_RELEASE(sym);
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
				key_press(event.key.keysym.sym);
				break;
			case SDL_KEYUP:
				key_release(event.key.keysym.sym);
				break;
			default:
				break;
		}
	}
	keyboard_column_update();
	keyboard_row_update();
}

#endif  /* HAVE_SDL_KEYBOARD */
