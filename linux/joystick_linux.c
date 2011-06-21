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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/joystick.h>

#include "types.h"
#include "events.h"
#include "input.h"
#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"
#include "xroar.h"

static int init(void);
static void shutdown(void);

JoystickModule joystick_linux_module = {
	.common = { .name = "linux", .description = "Linux joystick driver",
	            .init = init, .shutdown = shutdown }
};

/* Arbitrary limit: */
#define MAX_JOYSTICK_DEVICES (32)
static int num_joystick_devices;

static struct joy {
	int device_num;
	int fd;
	int num_axes;
	int num_buttons;
} joy[6];
static int num_joys = 0;

#define CONTROL_RIGHT_X    (0)
#define CONTROL_RIGHT_Y    (1)
#define CONTROL_LEFT_X     (2)
#define CONTROL_LEFT_Y     (3)
#define CONTROL_RIGHT_FIRE (4)
#define CONTROL_LEFT_FIRE  (5)

static int control_order[6] = {
	CONTROL_RIGHT_X, CONTROL_RIGHT_Y, CONTROL_RIGHT_FIRE,
	CONTROL_LEFT_X,  CONTROL_LEFT_Y,  CONTROL_LEFT_FIRE
};

static struct {
	int joy_num;
	int control_num;
	int invert;
} control[6];

static struct {
	int joy_num;
	int control_num;
	int invert;
} control_config[6] = {
	{ 1, 0, 0 }, /* Right X axis */
	{ 1, 1, 0 }, /* Right Y axis */
	{ 0, 0, 0 }, /* Left X axis */
	{ 0, 1, 0 }, /* Left Y axis */
	{ 1, 0, 0 }, /* Right firebutton */
	{ 0, 0, 0 }  /* Left firebutton */
};

static event_t *poll_event;
static void do_poll(void);

static int open_joystick(int device_num) {
	char buf[33];
	int fd;
	/* Try /dev/input/jsN and /dev/jsN */
	snprintf(buf, sizeof(buf), "/dev/input/js%d", device_num);
	fd = open(buf, O_NONBLOCK|O_RDONLY);
	if (fd < 0) {
		snprintf(buf, sizeof(buf), "/dev/js%d", device_num);
		fd = open(buf, O_RDONLY);
	}
	return fd;
}

static int find_joy(int device_num) {
	char namebuf[128];
	int i, j;
	if (device_num < 0)
		return -1;
	for (i = 0; i < num_joys; i++) {
		if (joy[i].device_num == device_num)
			return i;
	}
	i = num_joys;
	j = joy[i].fd = open_joystick(device_num);
	if (j < 0) {
		return -1;
	}
	num_joys++;
	joy[i].device_num = device_num;
	ioctl(j, JSIOCGNAME(sizeof(namebuf)), namebuf);
	ioctl(j, JSIOCGAXES, &joy[i].num_axes);
	ioctl(j, JSIOCGBUTTONS, &joy[i].num_buttons);
	LOG_DEBUG(1,"\t%s\n", namebuf);
	LOG_DEBUG(2,"\tNumber of Axes: %d\n", joy[i].num_axes);
	LOG_DEBUG(2,"\tNumber of Buttons: %d\n", joy[i].num_buttons);
	return i;
}

static void parse_joystick_def(char *def, int base) {
	int joy_num = control_config[base].joy_num;
	int i;
	if (def == NULL)
		return;
	for (i = 0; i < 3; i++) {
		char *tmp2 = strsep(&def, ":");
		char *tmp1 = strsep(&tmp2, ",");
		int control_num = i % 2;
		int invert = 0;
		int control_index = control_order[base+i];
		if (tmp2) {
			joy_num = atoi(tmp1);
			if (*tmp2) {
				if (*tmp2 == '-') {
					invert = 0xff;
					tmp2++;
				}
				if (*tmp2) {
					control_num = atoi(tmp2);
				}
			}
		} else if (tmp1) {
			if (*tmp1 == '-') {
				invert = 0xff;
				tmp1++;
			}
			if (*tmp1) {
				control_num = atoi(tmp1);
			}
		}
		control_config[control_index].joy_num = joy_num;
		control_config[control_index].control_num = control_num;
		control_config[control_index].invert = invert;
	}
}

static int init(void) {
	int valid, i;

	LOG_DEBUG(2,"Initialising Linux joystick driver\n");

	poll_event = event_new();
	if (poll_event == NULL) {
		LOG_WARN("Couldn't create joystick polling event.\n");
		return 1;
	}
	poll_event->dispatch = do_poll;

	/* Count how many joystick devices we can open. */
	num_joystick_devices = 0;
	for (i = 0; i < MAX_JOYSTICK_DEVICES; i++) {
		int fd;
		if ((fd = open_joystick(i)) >= 0) {
			num_joystick_devices++;
			close(fd);
		}
	}

	if (num_joystick_devices < 1) {
		LOG_WARN("No joysticks attached.\n");
		return 1;
	}

	/* If only one joystick attached, change the right joystick defaults */
	if (num_joystick_devices == 1) {
		control_config[INPUT_JOY_RIGHT_X].joy_num = control_config[INPUT_JOY_RIGHT_Y].joy_num
			= control_config[INPUT_JOY_RIGHT_FIRE].joy_num = 0;
		control_config[INPUT_JOY_RIGHT_X].control_num = 3;
		control_config[INPUT_JOY_RIGHT_Y].control_num = 2;
		control_config[INPUT_JOY_RIGHT_FIRE].control_num = 1;
	}

	if (xroar_opt_joy_right) {
		parse_joystick_def(xroar_opt_joy_right, 0);
	}
	if (xroar_opt_joy_left) {
		parse_joystick_def(xroar_opt_joy_left, 3);
	}

	valid = 0;
	for (i = 0; i < 6; i++) {
		int j = find_joy(control_config[i].joy_num);
		control[i].joy_num = j;
		control[i].control_num = control_config[i].control_num;
		control[i].invert = control_config[i].invert;
		if (j >= 0 && (i % 3) < 2) {
			if (control_config[i].control_num >= joy[j].num_axes) {
				LOG_WARN("Axis %d not found on joystick %d\n", control_config[i].control_num, control_config[i].joy_num);
				control[i].joy_num = -1;
			}
		} else if (j >= 0 && (i % 3) == 2) {
			if (control_config[i].control_num >= joy[j].num_buttons) {
				LOG_WARN("Button %d not found on joystick %d\n", control_config[i].control_num, control_config[i].joy_num);
				control[i].joy_num = -1;
			}
		} else {
			LOG_WARN("Joystick %d not found\n", control_config[i].joy_num);
			control[i].joy_num = -1;
		}
		if (control[i].joy_num >= 0)
			valid++;
	}

	/* No point scheduling joystick reads if we don't have any */
	if (valid) {
		poll_event->at_cycle = current_cycle + (OSCILLATOR_RATE / 100);
		event_queue(&UI_EVENT_LIST, poll_event);
	} else {
		LOG_WARN("No valid joystick mappings made.\n");
	}
	return 0;
}

static void shutdown(void) {
	int i;
	LOG_DEBUG(2,"Shutting down Linux joystick driver\n");
	for (i = 0; i < num_joys; i++) {
		close(joy[i].fd);
	}
	num_joys = 0;
	event_free(poll_event);
}

static void do_poll(void) {
	struct js_event e;
	int i, j;

	/* Scan joysticks */
	for (j = 0; j < num_joys; j++) {
		while (read(joy[j].fd, &e, sizeof(struct js_event)) == sizeof(struct js_event)) {
			switch (e.type) {
				case JS_EVENT_AXIS:
					for (i = 0; i < 4; i++) {
						if (control[i].joy_num == j && control[i].control_num == e.number) {
							input_control_press(i, ((e.value + 32768) >> 8) ^ control[i].invert);
						}
					}
					break;
				case JS_EVENT_BUTTON:
					if (control[CONTROL_RIGHT_FIRE].joy_num == j && control[CONTROL_RIGHT_FIRE].control_num == e.number) {
						if (e.value) {
							input_control_press(INPUT_JOY_RIGHT_FIRE, 0);
						} else {
							input_control_release(INPUT_JOY_RIGHT_FIRE, 0);
						}
					}
					if (control[CONTROL_LEFT_FIRE].joy_num == j && control[CONTROL_LEFT_FIRE].control_num == e.number) {
						if (e.value) {
							input_control_press(INPUT_JOY_LEFT_FIRE, 0);
						} else {
							input_control_release(INPUT_JOY_LEFT_FIRE, 0);
						}
					}
					break;
				default:
					break;
			}
		}
	}
	joystick_update();
	poll_event->at_cycle += OSCILLATOR_RATE / 100;
	event_queue(&UI_EVENT_LIST, poll_event);
}
