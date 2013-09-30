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

#include <fcntl.h>
#include <linux/joystick.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pl_glib.h"
#include "pl_string.h"

#include "events.h"
#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "xroar.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_axis *configure_axis(char *, unsigned);
static struct joystick_button *configure_button(char *, unsigned);
static void unmap_axis(struct joystick_axis *axis);
static void unmap_button(struct joystick_button *button);

static struct joystick_interface linux_js_if_physical = {
	.name = "physical",
	.configure_axis = configure_axis,
	.configure_button = configure_button,
	.unmap_axis = unmap_axis,
	.unmap_button = unmap_button,
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_interface *js_iflist[] = {
	&linux_js_if_physical,
	NULL
};

JoystickModule linux_js_mod = {
	.common = { .name = "linux", .description = "Linux joystick input" },
	.interface_list = js_iflist,
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct device {
	int joystick_index;
	int fd;
	unsigned open_count;
	unsigned num_axes;
	unsigned num_buttons;
	unsigned *axis_value;
	_Bool *button_value;
};

static GSList *device_list = NULL;

struct control {
	struct device *device;
	unsigned control;
	_Bool inverted;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct device *open_device(int joystick_index) {
	// If the device is already open, just up its count and return it
	for (GSList *iter = device_list; iter; iter = iter->next) {
		struct device *d = iter->data;
		if (d->joystick_index == joystick_index) {
			d->open_count++;
			return d;
		}
	}
	char buf[33];
	int fd;
	// Try /dev/input/jsN and /dev/jsN
	snprintf(buf, sizeof(buf), "/dev/input/js%d", joystick_index);
	fd = open(buf, O_NONBLOCK|O_RDONLY);
	if (fd < 0) {
		snprintf(buf, sizeof(buf), "/dev/js%d", joystick_index);
		fd = open(buf, O_RDONLY);
	}
	if (fd < 0)
		return NULL;
	struct device *d = g_malloc(sizeof(*d));
	d->joystick_index = joystick_index;
	d->fd = fd;
	char tmp;
	ioctl(fd, JSIOCGAXES, &tmp);
	d->num_axes = tmp;
	ioctl(fd, JSIOCGBUTTONS, &tmp);
	d->num_buttons = tmp;
	if (d->num_axes > 0)
		d->axis_value = g_malloc0(d->num_axes * sizeof(*d->axis_value));
	if (d->num_buttons > 0)
		d->button_value = g_malloc0(d->num_buttons * sizeof(*d->button_value));
	char namebuf[128];
	ioctl(fd, JSIOCGNAME(sizeof(namebuf)), namebuf);
	LOG_DEBUG(1, "Opened joystick %d: %s\n", joystick_index, namebuf);
	LOG_DEBUG(2, "\t%d axes, %d buttons\n", d->num_axes, d->num_buttons);
	d->open_count = 1;
	device_list = g_slist_prepend(device_list, d);
	return d;
}

static void close_device(struct device *d) {
	d->open_count--;
	if (d->open_count == 0) {
		close(d->fd);
		g_free(d->axis_value);
		g_free(d->button_value);
		device_list = g_slist_remove(device_list, d);
		g_free(d);
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void poll_devices(void) {
	for (GSList *iter = device_list; iter; iter = iter->next) {
		struct device *d = iter->data;
		struct js_event e;
		while (read(d->fd, &e, sizeof(e)) == sizeof(e)) {
			switch (e.type) {
			case JS_EVENT_AXIS:
				if (e.number < d->num_axes) {
					d->axis_value[e.number] = ((e.value) + 32768) >> 8;
				}
				break;
			case JS_EVENT_BUTTON:
				if (e.number < d->num_buttons) {
					d->button_value[e.number] = e.value;
				}
				break;
			default:
				break;
			}
		}
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static unsigned read_axis(struct control *c) {
	poll_devices();
	unsigned ret = c->device->axis_value[c->control];
	if (c->inverted)
		ret ^= 0xff;
	return ret;
}

static _Bool read_button(struct control *c) {
	poll_devices();
	return c->device->button_value[c->control];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// axis & button specs are basically the same, just track a different
// "selected" variable.
static struct control *configure_control(char *spec, unsigned control) {
	unsigned joystick = 0;
	_Bool inverted = 0;
	char *tmp = NULL;
	if (spec)
		tmp = strsep(&spec, ",");
	if (tmp && *tmp) {
		control = strtol(tmp, NULL, 0);
	}
	if (spec && *spec) {
		joystick = control;
		if (*spec == '-') {
			inverted = 1;
			spec++;
		}
		if (*spec) {
			control = strtol(spec, NULL, 0);
		}
	}
	struct device *d = open_device(joystick);
	if (!d)
		return NULL;
	struct control *c = g_malloc(sizeof(*c));
	c->device = d;
	c->control = control;
	c->inverted = inverted;
	return c;
}

static struct joystick_axis *configure_axis(char *spec, unsigned jaxis) {
	struct control *c = configure_control(spec, jaxis);
	if (!c)
		return NULL;
	if (c->control >= c->device->num_axes) {
		close_device(c->device);
		g_free(c);
		return NULL;
	}
	struct joystick_axis *axis = g_malloc(sizeof(*axis));
	axis->read = (js_read_axis_func)read_axis;
	axis->data = c;
	return axis;
}

static struct joystick_button *configure_button(char *spec, unsigned jbutton) {
	struct control *c = configure_control(spec, jbutton);
	if (!c)
		return NULL;
	if (c->control >= c->device->num_buttons) {
		close_device(c->device);
		g_free(c);
		return NULL;
	}
	struct joystick_button *button = g_malloc(sizeof(*button));
	button->read = (js_read_button_func)read_button;
	button->data = c;
	return button;
}

static void unmap_axis(struct joystick_axis *axis) {
	if (!axis)
		return;
	struct control *c = axis->data;
	close_device(c->device);
	g_free(c);
	g_free(axis);
}

static void unmap_button(struct joystick_button *button) {
	if (!button)
		return;
	struct control *c = button->data;
	close_device(c->device);
	g_free(c);
	g_free(button);
}
