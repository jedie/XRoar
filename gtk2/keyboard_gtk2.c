/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "input.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "printer.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);

KeyboardModule keyboard_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 keyboard input",
	            .init = init, .shutdown = shutdown }
};

extern GtkWidget *gtk2_top_window;
extern GtkUIManager *gtk2_menu_manager;
static gboolean keypress(GtkWidget *, GdkEventKey *, gpointer);
static gboolean keyrelease(GtkWidget *, GdkEventKey *, gpointer);

struct keymap {
	const char *name;
	unsigned int *raw;
};

#include "keyboard_gtk2_mappings.c"

static unsigned int emulate_joystick = 0;

#define MAX_KEYCODE (256)

/* For untranslated mode: unshifted keyvals for each keycode: */
static guint keycode_to_keyval[MAX_KEYCODE];

/* For translated mode: unicode value last generated for each keycode: */
static guint32 last_unicode[MAX_KEYCODE];

static void map_keyboard(unsigned int *map) {
	int i;
	GdkKeymap *gdk_keymap = gdk_keymap_get_for_display(gdk_display_get_default());
	/* Map keycode to unshifted keyval: */
	for (i = 0; i < MAX_KEYCODE; i++) {
		guint *keyvals;
		gint n_entries;
		if (gdk_keymap_get_entries_for_keycode(gdk_keymap, i, NULL, &keyvals, &n_entries) == TRUE) {
			if (n_entries > 0) {
				guint keyval = keyvals[0];
				/* Hack for certain GDK keyvals */
				if (keyval >= 0xff00 && keyval < 0xff20) {
					keycode_to_keyval[i] = keyval & 0xff;
				} else {
					keycode_to_keyval[i] = keyval;
				}
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
	/* Connect GTK key press/release signals to handlers */
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_press_event", G_CALLBACK(keypress), NULL);
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_release_event", G_CALLBACK(keyrelease), NULL);
	return 0;
}

static void shutdown(void) {
}

static void emulator_command(guint keyval, int shift) {
	switch (keyval) {
	case GDK_1: case GDK_2: case GDK_3: case GDK_4:
		if (shift) {
			xroar_new_disk(keyval - GDK_1);
		}
		break;
	case GDK_5: case GDK_6: case GDK_7: case GDK_8:
		if (shift) {
			xroar_select_write_back(keyval - GDK_5, XROAR_TOGGLE);
		} else {
			xroar_select_write_enable(keyval - GDK_5, XROAR_TOGGLE);
		}
		break;
	case GDK_a:
		xroar_select_cross_colour(XROAR_CYCLE);
		break;
	case GDK_e:
		xroar_set_cart(XROAR_TOGGLE);
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
		xroar_set_keymap(XROAR_CYCLE);
		break;
	case GDK_m:
		xroar_set_machine(XROAR_CYCLE);
		break;
	case GDK_p:
		if (shift)
			printer_flush();
		break;
	case GDK_w:
		xroar_select_tape_output();
		break;
#ifdef TRACE
	case GDK_v:
		xroar_set_trace(XROAR_TOGGLE);  /* toggle */
		break;
#endif
	default:
		break;
	}
	return;
}

static gboolean keypress(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	int control, shift;
	(void)widget;
	(void)user_data;
	if (gtk_window_activate_key(GTK_WINDOW(gtk2_top_window), event) == TRUE) {
		return TRUE;
	}
	if (event->hardware_keycode >= MAX_KEYCODE) {
		return FALSE;
	}
	guint keyval = keycode_to_keyval[event->hardware_keycode];
	control = event->state & GDK_CONTROL_MASK;
	if (emulate_joystick) {
		if (keyval == GDK_Up) { input_control_press(INPUT_JOY_RIGHT_Y, 0); return FALSE; }
		if (keyval == GDK_Down) { input_control_press(INPUT_JOY_RIGHT_Y, 255); return FALSE; }
		if (keyval == GDK_Left) { input_control_press(INPUT_JOY_RIGHT_X, 0); return FALSE; }
		if (keyval == GDK_Right) { input_control_press(INPUT_JOY_RIGHT_X, 255); return FALSE; }
		if (keyval == GDK_Alt_L) { input_control_press(INPUT_JOY_RIGHT_FIRE, 0); return FALSE; }
	}
	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		KEYBOARD_PRESS_SHIFT();
		return FALSE;
	}
	shift = event->state & GDK_SHIFT_MASK;
	if (!shift) {
		KEYBOARD_RELEASE_SHIFT();
	}
	if (keyval == GDK_F12 && !xroar_noratelimit) {
		xroar_noratelimit = 1;
		xroar_frameskip = 10;
	}
	if (control) {
		emulator_command(keyval, shift);
		return FALSE;
	}
	if (keyval == GDK_Up) { KEYBOARD_PRESS(KEYMAP_UP); return FALSE; }
	if (keyval == GDK_Down) { KEYBOARD_PRESS(KEYMAP_DOWN); return FALSE; }
	if (keyval == GDK_Left) { KEYBOARD_PRESS(KEYMAP_LEFT); return FALSE; }
	if (keyval == GDK_Right) { KEYBOARD_PRESS(KEYMAP_RIGHT); return FALSE; }
	if (keyval == GDK_Home) { KEYBOARD_PRESS(KEYMAP_CLEAR); return FALSE; }
	if (xroar_kbd_translate) {
		guint32 unicode;
		guint16 keycode = event->hardware_keycode;
		if (event->keyval >= 0xff00 && event->keyval < 0xff20) {
			/* Hack for certain GDK keyvals */
			unicode = event->keyval & 0xff;
		} else {
			unicode = gdk_keyval_to_unicode(event->keyval);
		}
		/* Map shift + backspace/delete to ^U */
		if (shift && (unicode == 8 || unicode == 127)) {
			unicode = 0x15;
		}
		last_unicode[keycode] = unicode;
		keyboard_unicode_press(unicode);
		return FALSE;
	}
	if (keyval < 128) {
		KEYBOARD_PRESS(keyval);
	}
	return FALSE;
}

static gboolean keyrelease(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	int shift;
	(void)widget;
	(void)user_data;
	if (event->hardware_keycode >= MAX_KEYCODE) {
		return FALSE;
	}
	guint keyval = keycode_to_keyval[event->hardware_keycode];
	if (emulate_joystick) {
		if (keyval == GDK_Up) { input_control_release(INPUT_JOY_RIGHT_Y, 0); return FALSE; }
		if (keyval == GDK_Down) { input_control_release(INPUT_JOY_RIGHT_Y, 255); return FALSE; }
		if (keyval == GDK_Left) { input_control_release(INPUT_JOY_RIGHT_X, 0); return FALSE; }
		if (keyval == GDK_Right) { input_control_release(INPUT_JOY_RIGHT_X, 255); return FALSE; }
		if (keyval == GDK_Alt_L) { input_control_release(INPUT_JOY_RIGHT_FIRE, 0); return FALSE; }
	}
	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		KEYBOARD_RELEASE_SHIFT();
		return FALSE;
	}
	shift = event->state & GDK_SHIFT_MASK;
	if (!shift) {
		KEYBOARD_RELEASE_SHIFT();
	}
	if (keyval == GDK_F12) {
		xroar_noratelimit = 0;
		xroar_frameskip = xroar_opt_frameskip;
	}
	if (keyval == GDK_Up) { KEYBOARD_RELEASE(KEYMAP_UP); return FALSE; }
	if (keyval == GDK_Down) { KEYBOARD_RELEASE(KEYMAP_DOWN); return FALSE; }
	if (keyval == GDK_Left) { KEYBOARD_RELEASE(KEYMAP_LEFT); return FALSE; }
	if (keyval == GDK_Right) { KEYBOARD_RELEASE(KEYMAP_RIGHT); return FALSE; }
	if (keyval == GDK_Home) { KEYBOARD_RELEASE(KEYMAP_CLEAR); return FALSE; }
	if (xroar_kbd_translate) {
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
			KEYBOARD_PRESS_SHIFT();
		return FALSE;
	}
	if (keyval < 128) {
		KEYBOARD_RELEASE(keyval);
	}
	return FALSE;
}
