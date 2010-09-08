/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "types.h"
#include "logging.h"
#include "events.h"
#include "input.h"
#include "keyboard.h"
#include "machine.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);

KeyboardModule keyboard_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 keyboard driver",
	            .init = init, .shutdown = shutdown }
};

extern GtkWidget *gtk2_top_window;
extern GtkRadioAction *gtk2_radio_machine;
static gboolean keypress(GtkWidget *, GdkEventKey *, gpointer);
static gboolean keyrelease(GtkWidget *, GdkEventKey *, gpointer);

struct keymap {
	const char *name;
	unsigned int *raw;
};

#include "keyboard_gtk2_mappings.c"

static unsigned int control = 0, shift = 0;
static unsigned int emulate_joystick = 0;
static int old_frameskip = 0;

#define MAX_KEYCODE (256)

/* For untranslated mode: unshifted keyvals for each keycode: */
static guint keycode_to_keyval[MAX_KEYCODE];

/* For translated mode: unicode value last generated for each keycode: */
static guint32 last_unicode[MAX_KEYCODE];

static int translated_keymap;

static void map_keyboard(unsigned int *map) {
	int i;
	/* Map keycode to unshifted keyval: */
	for (i = 0; i < MAX_KEYCODE; i++) {
		guint *keyvals;
		gint n_entries;
		if (gdk_keymap_get_entries_for_keycode(NULL, i, NULL, &keyvals, &n_entries) == TRUE) {
			if (n_entries > 0) {
				guint keyval = keyvals[0];
				if (keyval >= 0xff00 && keyval < 0xff20) {
					keycode_to_keyval[i] = keyval & 0xff;
					continue;
				}
				keycode_to_keyval[i] = keyval;
			}
			g_free(keyvals);
		} else {
			keycode_to_keyval[i] = 0;
		}
	}
	/* Initialise keycode->unicode tracking: */
	for (i = 0; i < MAX_KEYCODE; i++) {
		last_unicode[i] = 0;
	}
	/* Apply keyboard map if selected: */
	if (map == NULL)
		return;
	while (*map) {
		guint old_keyval = *(map++);
		guint new_keyval = *(map++);
		GdkKeymapKey *keys;
		gint n_keys;
		/* Find keycode that generates old_keyval when unshifted,
		   and map it to new_keyval: */
		if (gdk_keymap_get_entries_for_keyval(NULL, old_keyval, &keys, &n_keys) == TRUE) {
			for (i = 0; i < n_keys; i++) {
				guint keycode = keys[i].keycode;
				if (keycode < MAX_KEYCODE && keys[i].group == 0 && keys[i].level == 0) {
					keycode_to_keyval[keycode] = new_keyval;
				}
			}
			g_free(keys);
		}
	}
}

static int init(void) {
	static unsigned int *selected_keymap = NULL;
	int i;
	for (i = 0; mappings[i].name; i++) {
		if (selected_keymap == NULL
				&& !strcmp("uk", mappings[i].name)) {
			selected_keymap = mappings[i].raw;
		}
		if (xroar_opt_keymap && !strcmp(xroar_opt_keymap, mappings[i].name)) {
			selected_keymap = mappings[i].raw;
			LOG_DEBUG(2,"\tSelecting '%s' keymap\n", xroar_opt_keymap);
		}
	}
	map_keyboard(selected_keymap);
	translated_keymap = 0;
	/* Connect GTK key press/release signals to handlers */
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_press_event", G_CALLBACK(keypress), NULL);
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_release_event", G_CALLBACK(keyrelease), NULL);
	return 0;
}

static void shutdown(void) {
}

static void emulator_command(guint keyval) {
	switch (keyval) {
	case GDK_1: case GDK_2: case GDK_3: case GDK_4:
		if (shift) {
			xroar_new_disk(keyval - GDK_1);
		} else {
			xroar_insert_disk(keyval - GDK_1);
		}
		break;
	case GDK_5: case GDK_6: case GDK_7: case GDK_8:
		if (shift) {
			xroar_toggle_write_back(keyval - GDK_5);
		} else {
			xroar_toggle_write_protect(keyval - GDK_5);
		}
		break;
	case GDK_a:
		xroar_cycle_cross_colour();
		break;
	case GDK_e:
		xroar_dos_enable(XROAR_TOGGLE);
		break;
	case GDK_f:
		xroar_fullscreen(XROAR_TOGGLE);
		break;
	case GDK_j:
		if (shift) {
			input_control_press(INPUT_SWAP_JOYSTICKS, 0);
		} else {
			if (emulate_joystick) {
				input_control_press(INPUT_SWAP_JOYSTICKS, 0);
			}
			emulate_joystick = (emulate_joystick + 1) % 3;
		}
		break;
	case GDK_k:
		xroar_cycle_keymap();
		break;
	case GDK_m:
		gtk_radio_action_set_current_value(gtk2_radio_machine, (running_machine + 1) % NUM_MACHINE_TYPES);
		break;
	case GDK_s:
		xroar_save_snapshot();
		break;
	case GDK_w:
		xroar_write_tape();
		break;
#ifdef TRACE
	case GDK_v:
		xroar_set_trace(XROAR_TOGGLE);  /* toggle */
		break;
#endif
	case GDK_z: // running out of letters...
		translated_keymap = !translated_keymap;
		break;
	default:
		break;
	}
	return;
}

static gboolean keypress(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	(void)widget;
	(void)user_data;
	if (gtk_window_activate_key(GTK_WINDOW(gtk2_top_window), event) == TRUE) {
		return TRUE;
	}
	if (event->keyval == GDK_F11) {
		xroar_fullscreen(XROAR_TOGGLE);
		return TRUE;
	}
	if (event->hardware_keycode >= MAX_KEYCODE) {
		return TRUE;
	}
	guint keyval = keycode_to_keyval[event->hardware_keycode];
	if (emulate_joystick) {
		if (keyval == GDK_Up) { input_control_press(INPUT_JOY_RIGHT_Y, 0); return TRUE; }
		if (keyval == GDK_Down) { input_control_press(INPUT_JOY_RIGHT_Y, 255); return TRUE; }
		if (keyval == GDK_Left) { input_control_press(INPUT_JOY_RIGHT_X, 0); return TRUE; }
		if (keyval == GDK_Right) { input_control_press(INPUT_JOY_RIGHT_X, 255); return TRUE; }
		if (keyval == GDK_Alt_L) { input_control_press(INPUT_JOY_RIGHT_FIRE, 0); return TRUE; }
	}
	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		shift = 1;
		KEYBOARD_PRESS(0);
		return TRUE;
	}
	if (keyval == GDK_Control_L || keyval == GDK_Control_R) { control = 1; return TRUE; }
	if (keyval == GDK_F12 && !xroar_noratelimit) {
		xroar_noratelimit = 1;
		old_frameskip = xroar_frameskip;
		xroar_frameskip = 10;
	}
	if (control) {
		emulator_command(keyval);
		return TRUE;
	}
	if (keyval == GDK_Up) { KEYBOARD_PRESS(94); return TRUE; }
	if (keyval == GDK_Down) { KEYBOARD_PRESS(10); return TRUE; }
	if (keyval == GDK_Left) { KEYBOARD_PRESS(8); return TRUE; }
	if (keyval == GDK_Right) { KEYBOARD_PRESS(9); return TRUE; }
	if (keyval == GDK_Home) { KEYBOARD_PRESS(12); return TRUE; }
	if (translated_keymap) {
		guint32 unicode;
		guint16 keycode = event->hardware_keycode;
		unicode = gdk_keyval_to_unicode(event->keyval);
		/* Map shift + backspace/delete to ^U */
		if (shift && (unicode == 8 || unicode == 127)) {
			unicode = 0x15;
		}
		last_unicode[keycode] = unicode;
		keyboard_unicode_press(unicode);
		return TRUE;
	}
	if (keyval < 128) {
		KEYBOARD_PRESS(keyval);
	}
	return TRUE;
}

static gboolean keyrelease(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	(void)widget;
	(void)user_data;
	if (event->hardware_keycode >= MAX_KEYCODE) {
		return TRUE;
	}
	guint keyval = keycode_to_keyval[event->hardware_keycode];
	if (emulate_joystick) {
		if (keyval == GDK_Up) { input_control_release(INPUT_JOY_RIGHT_Y, 0); return TRUE; }
		if (keyval == GDK_Down) { input_control_release(INPUT_JOY_RIGHT_Y, 255); return TRUE; }
		if (keyval == GDK_Left) { input_control_release(INPUT_JOY_RIGHT_X, 0); return TRUE; }
		if (keyval == GDK_Right) { input_control_release(INPUT_JOY_RIGHT_X, 255); return TRUE; }
		if (keyval == GDK_Alt_L) { input_control_release(INPUT_JOY_RIGHT_FIRE, 0); return TRUE; }
	}
	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		shift = 0;
		KEYBOARD_RELEASE(0);
		return TRUE;
	}
	if (keyval == GDK_Control_L || keyval == GDK_Control_R) { control = 0; return TRUE; }
	if (keyval == GDK_F12) {
		xroar_noratelimit = 0;
		xroar_frameskip = old_frameskip;
	}
	if (keyval == GDK_Up) { KEYBOARD_RELEASE(94); return TRUE; }
	if (keyval == GDK_Down) { KEYBOARD_RELEASE(10); return TRUE; }
	if (keyval == GDK_Left) { KEYBOARD_RELEASE(8); return TRUE; }
	if (keyval == GDK_Right) { KEYBOARD_RELEASE(9); return TRUE; }
	if (keyval == GDK_Home) { KEYBOARD_RELEASE(12); return TRUE; }
	if (translated_keymap) {
		guint32 unicode;
		guint16 keycode = event->hardware_keycode;
		unicode = last_unicode[keycode];
		/* Map shift + backspace/delete to ^U */
		if (shift && (unicode == 8 || unicode == 127)) {
			unicode = 0x15;
		}
		keyboard_unicode_release(unicode);
		/* Might have unpressed shift prematurely */
		if (shift)
			KEYBOARD_PRESS(0);
		return TRUE;
	}
	if (keyval < 128) {
		KEYBOARD_RELEASE(keyval);
	}
	return TRUE;
}
