/*  Copyright 2003-2014 Ciaran Anscomb
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

#include <stdlib.h>
#include <string.h>

#include "pl-string.h"
#include "slist.h"
#include "xalloc.h"

#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "module.h"

extern struct joystick_module linux_js_mod;
extern struct joystick_module sdl_js_mod_exported;
static struct joystick_module * const joystick_module_list[] = {
#ifdef HAVE_LINUX_JOYSTICK
	&linux_js_mod,
#endif
#ifdef HAVE_SDL
	&sdl_js_mod_exported,
#endif
	NULL
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct slist *config_list = NULL;
static unsigned num_configs = 0;

// Current configuration, per-port:
struct joystick_config const *joystick_port_config[JOYSTICK_NUM_PORTS];

static struct joystick_interface *selected_interface = NULL;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct joystick {
	struct joystick_axis *axes[JOYSTICK_NUM_AXES];
	struct joystick_button *buttons[JOYSTICK_NUM_BUTTONS];
};

static struct joystick *joystick_port[JOYSTICK_NUM_PORTS];

// Support the swap/cycle shortcuts:
static struct joystick_config const *virtual_joystick_config;
static struct joystick const *virtual_joystick = NULL;
static struct joystick_config const *cycled_config[JOYSTICK_NUM_PORTS];

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void joystick_init(void) {
	for (unsigned p = 0; p < JOYSTICK_NUM_PORTS; p++) {
		joystick_port[p] = NULL;
		cycled_config[p] = NULL;
	}
}

void joystick_shutdown(void) {
	for (unsigned p = 0; p < JOYSTICK_NUM_PORTS; p++) {
		joystick_unmap(p);
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct joystick_config *joystick_config_new(void) {
	struct joystick_config *new;
	new = xzalloc(sizeof(*new));
	new->index = num_configs;
	config_list = slist_append(config_list, new);
	num_configs++;
	return new;
}

unsigned joystick_config_count(void) {
	return num_configs;
}

struct joystick_config *joystick_config_index(unsigned i) {
	for (struct slist *l = config_list; l; l = l->next) {
		struct joystick_config *jc = l->data;
		if (jc->index == i)
			return jc;
	}
	return NULL;
}

struct joystick_config *joystick_config_by_name(const char *name) {
	if (!name) return NULL;
	for (struct slist *l = config_list; l; l = l->next) {
		struct joystick_config *jc = l->data;
		if (0 == strcmp(jc->name, name)) {
			return jc;
		}
	}
	return NULL;
}

void joystick_config_print_all(void) {
	for (struct slist *l = config_list; l; l = l->next) {
		struct joystick_config *jc = l->data;
		printf("joy %s\n", jc->name);
		if (jc->description) printf("  joy-desc %s\n", jc->description);
		for (int i = 0 ; i < JOYSTICK_NUM_AXES; i++) {
			if (jc->axis_specs[i])
				printf("  joy-axis %d=%s\n", i, jc->axis_specs[i]);
		}
		for (int i = 0 ; i < JOYSTICK_NUM_BUTTONS; i++) {
			if (jc->button_specs[i])
				printf("  joy-button %d=%s\n", i, jc->button_specs[i]);
		}
		printf("\n");
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_interface *find_if_in_mod(struct joystick_module *module, const char *if_name) {
	if (!module || !if_name)
		return NULL;
	for (unsigned i = 0; module->intf_list[i]; i++) {
		if (0 == strcmp(module->intf_list[i]->name, if_name))
			return module->intf_list[i];
	}
	return NULL;
}

static struct joystick_interface *find_if_in_modlist(struct joystick_module * const *list, const char *if_name) {
	if (!list || !if_name)
		return NULL;
	for (unsigned i = 0; list[i]; i++) {
		struct joystick_interface *intf = find_if_in_mod(list[i], if_name);
		if (intf)
			return intf;
	}
	return NULL;
}

static struct joystick_interface *find_if(const char *if_name) {
	struct joystick_interface *intf;
	if ((intf = find_if_in_modlist(ui_module->joystick_module_list, if_name)))
		return intf;
	return find_if_in_modlist(joystick_module_list, if_name);
}

static void select_interface(char **spec) {
	char *mod_name = NULL;
	char *if_name = NULL;
	if (*spec && strchr(*spec, ':')) {
		if_name = strsep(spec, ":");
	}
	if (*spec && strchr(*spec, ':')) {
		mod_name = if_name;
		if_name = strsep(spec, ":");
	}
	if (mod_name) {
		struct joystick_module *m = (struct joystick_module *)module_select_by_arg((struct module **)ui_module->joystick_module_list, mod_name);
		if (!m) {
			m = (struct joystick_module *)module_select_by_arg((struct module **)joystick_module_list, mod_name);
		}
		selected_interface = find_if_in_mod(m, if_name);
	} else if (if_name) {
		selected_interface = find_if(if_name);
	} else if (!selected_interface) {
		selected_interface = find_if("physical");
	}
}

void joystick_map(struct joystick_config const *jc, unsigned port) {
	selected_interface = NULL;
	if (port >= JOYSTICK_NUM_PORTS)
		return;
	joystick_unmap(port);
	if (!jc)
		return;
	struct joystick *j = xzalloc(sizeof(*j));
	_Bool valid_joystick = 0;
	for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++) {
		char *spec_copy = xstrdup(jc->axis_specs[i]);
		char *spec = spec_copy;
		select_interface(&spec);
		if (!selected_interface) {
			free(spec_copy);
			return;
		}
		struct joystick_axis *axis = selected_interface->configure_axis(spec, i);
		j->axes[i] = axis;
		if (axis) {
			axis->intf = selected_interface;
			valid_joystick = 1;
		}
		free(spec_copy);
	}
	for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++) {
		char *spec_copy = xstrdup(jc->button_specs[i]);
		char *spec = spec_copy;
		select_interface(&spec);
		if (!selected_interface) {
			free(spec_copy);
			return;
		}
		struct joystick_button *button = selected_interface->configure_button(spec, i);
		j->buttons[i] = button;
		if (button) {
			button->intf = selected_interface;
			valid_joystick = 1;
		}
		free(spec_copy);
	}
	if (!valid_joystick) {
		free(j);
		return;
	}
	LOG_DEBUG(1, "Joystick port %u = %s [ ", port, jc->name);
	for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++) {
		if (j->axes[i])
			LOG_DEBUG(1, "%u=%s:", i, j->axes[i]->intf->name);
		LOG_DEBUG(1, ", ");
	}
	for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++) {
		if (j->buttons[i])
			LOG_DEBUG(1, "%u=%s:", i, j->buttons[i]->intf->name);
		if ((i + 1) < JOYSTICK_NUM_BUTTONS)
			LOG_DEBUG(1, ", ");
	}
	LOG_DEBUG(1, " ]\n");
	joystick_port[port] = j;
	joystick_port_config[port] = jc;
}

void joystick_unmap(unsigned port) {
	if (port >= JOYSTICK_NUM_PORTS)
		return;
	struct joystick *j = joystick_port[port];
	joystick_port_config[port] = NULL;
	joystick_port[port] = NULL;
	if (!j)
		return;
	for (unsigned a = 0; a < JOYSTICK_NUM_AXES; a++) {
		struct joystick_axis *axis = j->axes[a];
		if (axis) {
			struct joystick_interface *interface = axis->intf;
			if (interface->unmap_axis) {
				interface->unmap_axis(axis);
			} else {
				free(j->axes[a]);
			}
		}
	}
	for (unsigned b = 0; b < JOYSTICK_NUM_BUTTONS; b++) {
		struct joystick_button *button = j->buttons[b];
		if (button) {
			struct joystick_interface *interface = button->intf;
			if (interface->unmap_button) {
				interface->unmap_button(button);
			} else {
				free(j->buttons[b]);
			}
		}
	}
	free(j);
}

void joystick_set_virtual(struct joystick_config const *jc) {
	int remap_virtual_to = -1;
	if (virtual_joystick) {
		if (joystick_port[0] == virtual_joystick) {
			joystick_unmap(0);
			remap_virtual_to = 0;
		}
		if (joystick_port[1] == virtual_joystick) {
			joystick_unmap(1);
			remap_virtual_to = 1;
		}
	}
	virtual_joystick_config = jc;
	if (remap_virtual_to >= 0)
		joystick_map(jc, remap_virtual_to);
}

// Swap the right & left joysticks
void joystick_swap(void) {
	struct joystick_config const *tmp_jc = joystick_port_config[0];
	joystick_port_config[0] = joystick_port_config[1];
	joystick_port_config[1] = tmp_jc;
	struct joystick_config const *tmp_cc = cycled_config[0];
	cycled_config[0] = cycled_config[1];
	cycled_config[1] = tmp_cc;
	struct joystick *tmp_j = joystick_port[0];
	joystick_port[0] = joystick_port[1];
	joystick_port[1] = tmp_j;
}

// Cycle the virtual joystick through right and left joystick ports
void joystick_cycle(void) {
	if (!virtual_joystick_config) {
		joystick_swap();
		return;
	}
	if (virtual_joystick && joystick_port[0] == virtual_joystick) {
		cycled_config[1] = joystick_port_config[1];
		joystick_unmap(0);
		joystick_unmap(1);
		joystick_map(cycled_config[0], 0);
		joystick_map(virtual_joystick_config, 1);
		virtual_joystick = joystick_port[1];
	} else if (virtual_joystick && joystick_port[1] == virtual_joystick) {
		joystick_unmap(1);
		joystick_map(cycled_config[1], 1);
		virtual_joystick = NULL;
	} else {
		cycled_config[0] = joystick_port_config[0];
		joystick_unmap(0);
		joystick_map(virtual_joystick_config, 0);
		virtual_joystick = joystick_port[0];
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int joystick_read_axis(int port, int axis) {
	struct joystick *j = joystick_port[port];
	if (j && j->axes[axis]) {
		return j->axes[axis]->read(j->axes[axis]->data);
	}
	return 127;
}

int joystick_read_buttons(void) {
	int buttons = 0;
	if (joystick_port[0] && joystick_port[0]->buttons[0]) {
		if (joystick_port[0]->buttons[0]->read(joystick_port[0]->buttons[0]->data))
			buttons |= 1;
	}
	if (joystick_port[1] && joystick_port[1]->buttons[0]) {
		if (joystick_port[1]->buttons[0]->read(joystick_port[1]->buttons[0]->data))
			buttons |= 2;
	}
	return buttons;
}
