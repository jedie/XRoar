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

// For strsep()
#define _BSD_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "pl_string.h"

#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "module.h"
#include "printer.h"
#include "xroar.h"

#include "gtk2/ui_gtk2.h"

static _Bool init(void);
static void shutdown(void);

KeyboardModule keyboard_gtk2_module = {
	.common = { .name = "gtk2", .description = "GTK+-2 keyboard input",
	            .init = init, .shutdown = shutdown }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_axis *configure_axis(char *, unsigned);
static struct joystick_button *configure_button(char *, unsigned);
static void unmap_axis(struct joystick_axis *axis);
static void unmap_button(struct joystick_button *button);

struct joystick_interface gtk2_js_if_keyboard = {
	.name = "keyboard",
	.configure_axis = configure_axis,
	.configure_button = configure_button,
	.unmap_axis = unmap_axis,
	.unmap_button = unmap_button,
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct axis {
	unsigned key0, key1;
	unsigned value;
};

struct button {
	unsigned key;
	_Bool value;
};

#define MAX_AXES (4)
#define MAX_BUTTONS (4)

static struct axis *enabled_axis[MAX_AXES];
static struct button *enabled_button[MAX_BUTTONS];

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static gboolean keypress(GtkWidget *, GdkEventKey *, gpointer);
static gboolean keyrelease(GtkWidget *, GdkEventKey *, gpointer);

struct keymap {
	const char *name;
	unsigned int *raw;
};

#include "keyboard_gtk2_mappings.c"

static _Bool noratelimit_latch = 0;

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

static _Bool init(void) {
	static unsigned int *selected_keymap = NULL;
	int i;
	for (i = 0; mappings[i].name; i++) {
		if (selected_keymap == NULL
				&& !strcmp("uk", mappings[i].name)) {
			selected_keymap = mappings[i].raw;
		}
		if (xroar_cfg.keymap && !strcmp(xroar_cfg.keymap, mappings[i].name)) {
			selected_keymap = mappings[i].raw;
			LOG_DEBUG(2,"\tSelecting '%s' keymap\n", xroar_cfg.keymap);
		}
	}
	map_keyboard(selected_keymap);
	/* Connect GTK key press/release signals to handlers */
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_press_event", G_CALLBACK(keypress), NULL);
	g_signal_connect(G_OBJECT(gtk2_top_window), "key_release_event", G_CALLBACK(keyrelease), NULL);
	return 1;
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
		xroar_toggle_cart();
		break;
	case GDK_f:
		xroar_fullscreen(XROAR_TOGGLE);
		break;
	case GDK_j:
		if (shift) {
			joystick_swap();
		} else {
			joystick_cycle();
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

	for (unsigned i = 0; i < MAX_AXES; i++) {
		if (enabled_axis[i]) {
			if (keyval == enabled_axis[i]->key0) {
				enabled_axis[i]->value = 0;
				return FALSE;
			}
			if (keyval == enabled_axis[i]->key1) {
				enabled_axis[i]->value = 255;
				return FALSE;
			}
		}
	}
	for (unsigned i = 0; i < MAX_BUTTONS; i++) {
		if (enabled_button[i]) {
			if (keyval == enabled_button[i]->key) {
				enabled_button[i]->value = 1;
				return FALSE;
			}
		}
	}

	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		KEYBOARD_PRESS_SHIFT;
		return FALSE;
	}
	shift = event->state & GDK_SHIFT_MASK;
	if (!shift) {
		KEYBOARD_RELEASE_SHIFT;
	}
	if (keyval == GDK_F12 && !xroar_noratelimit) {
		if (shift) {
			noratelimit_latch = !noratelimit_latch;
			if (noratelimit_latch) {
				xroar_noratelimit = 1;
				xroar_frameskip = 10;
			} else {
				xroar_noratelimit = 0;
				xroar_frameskip = xroar_cfg.frameskip;
			}
		} else {
			xroar_noratelimit = 1;
			xroar_frameskip = 10;
		}
	}
	if (keyval == 0x13) { machine_toggle_pause(); return FALSE; }
	if (control) {
		emulator_command(keyval, shift);
		return FALSE;
	}
	if (keyval == GDK_Up) { KEYBOARD_PRESS(KEYMAP_UP); return FALSE; }
	if (keyval == GDK_Down) { KEYBOARD_PRESS(KEYMAP_DOWN); return FALSE; }
	if (keyval == GDK_Left) { KEYBOARD_PRESS(KEYMAP_LEFT); return FALSE; }
	if (keyval == GDK_Right) { KEYBOARD_PRESS(KEYMAP_RIGHT); return FALSE; }
	if (keyval == GDK_Home) { KEYBOARD_PRESS_CLEAR; return FALSE; }
	if (xroar_cfg.kbd_translate) {
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

	for (unsigned i = 0; i < MAX_AXES; i++) {
		if (enabled_axis[i]) {
			if (keyval == enabled_axis[i]->key0) {
				if (enabled_axis[i]->value < 129)
					enabled_axis[i]->value = 129;
				return FALSE;
			}
			if (keyval == enabled_axis[i]->key1) {
				if (enabled_axis[i]->value > 130)
					enabled_axis[i]->value = 130;
				return FALSE;
			}
		}
	}
	for (unsigned i = 0; i < MAX_BUTTONS; i++) {
		if (enabled_button[i]) {
			if (keyval == enabled_button[i]->key) {
				enabled_button[i]->value = 0;
				return FALSE;
			}
		}
	}

	if (keyval == GDK_Shift_L || keyval == GDK_Shift_R) {
		KEYBOARD_RELEASE_SHIFT;
		return FALSE;
	}
	shift = event->state & GDK_SHIFT_MASK;
	if (!shift) {
		KEYBOARD_RELEASE_SHIFT;
	}
	if (keyval == GDK_F12) {
		if (!noratelimit_latch) {
			xroar_noratelimit = 0;
			xroar_frameskip = xroar_cfg.frameskip;
		}
	}
	if (keyval == GDK_Up) { KEYBOARD_RELEASE(KEYMAP_UP); return FALSE; }
	if (keyval == GDK_Down) { KEYBOARD_RELEASE(KEYMAP_DOWN); return FALSE; }
	if (keyval == GDK_Left) { KEYBOARD_RELEASE(KEYMAP_LEFT); return FALSE; }
	if (keyval == GDK_Right) { KEYBOARD_RELEASE(KEYMAP_RIGHT); return FALSE; }
	if (keyval == GDK_Home) { KEYBOARD_RELEASE_CLEAR; return FALSE; }
	if (xroar_cfg.kbd_translate) {
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
			KEYBOARD_PRESS_SHIFT;
		return FALSE;
	}
	if (keyval < 128) {
		KEYBOARD_RELEASE(keyval);
	}
	return FALSE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static unsigned read_axis(struct axis *a) {
	return a->value;
}

static _Bool read_button(struct button *b) {
	return b->value;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static unsigned get_key_by_name(const char *name) {
	if (isdigit(name[0]))
		return strtol(name, NULL, 0);
	return gdk_keyval_from_name(name);
}

static struct joystick_axis *configure_axis(char *spec, unsigned jaxis) {
	unsigned key0, key1;
	// sensible defaults
	if (jaxis == 0) {
		key0 = GDK_Left;
		key1 = GDK_Right;
	} else {
		key0 = GDK_Up;
		key1 = GDK_Down;
	}
	char *a0 = NULL;
	char *a1 = NULL;
	if (spec) {
		a0 = strsep(&spec, ",");
		a1 = spec;
	}
	if (a0 && *a0)
		key0 = get_key_by_name(a0);
	if (a1 && *a1)
		key1 = get_key_by_name(a1);
	struct axis *axis_data = g_malloc(sizeof(*axis_data));
	axis_data->key0 = key0;
	axis_data->key1 = key1;
	axis_data->value = 127;
	struct joystick_axis *axis = g_malloc(sizeof(*axis));
	axis->read = (js_read_axis_func)read_axis;
	axis->data = axis_data;
	for (unsigned i = 0; i < MAX_AXES; i++) {
		if (!enabled_axis[i]) {
			enabled_axis[i] = axis_data;
			break;
		}
	}
	return axis;
}

static struct joystick_button *configure_button(char *spec, unsigned jbutton) {
	unsigned key = (jbutton == 0) ? GDK_Alt_L : GDK_VoidSymbol;
	if (spec && *spec)
		key = get_key_by_name(spec);
	struct button *button_data = g_malloc(sizeof(*button_data));
	button_data->key = key;
	button_data->value = 0;
	struct joystick_button *button = g_malloc(sizeof(*button));
	button->read = (js_read_button_func)read_button;
	button->data = button_data;
	for (unsigned i = 0; i < MAX_BUTTONS; i++) {
		if (!enabled_button[i]) {
			enabled_button[i] = button_data;
			break;
		}
	}
	return button;
}

static void unmap_axis(struct joystick_axis *axis) {
	if (!axis)
		return;
	for (unsigned i = 0; i < MAX_AXES; i++) {
		if (axis->data == enabled_axis[i]) {
			enabled_axis[i] = NULL;
		}
	}
	g_free(axis->data);
	g_free(axis);
}

static void unmap_button(struct joystick_button *button) {
	if (!button)
		return;
	for (unsigned i = 0; i < MAX_BUTTONS; i++) {
		if (button->data == enabled_button[i]) {
			enabled_button[i] = NULL;
		}
	}
	g_free(button->data);
	g_free(button);
}
