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

// For strsep()
#define _BSD_SOURCE

#include "portalib/glib.h"
#include "portalib/string.h"

#include "joystick.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "module.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static GSList *config_list = NULL;
static unsigned num_configs = 0;

// Current configuration, per-port:
struct joystick_config *joystick_port_config[JOYSTICK_NUM_PORTS];

static struct joystick_interface *selected_interface = NULL;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct joystick {
	struct joystick_axis *axes[JOYSTICK_NUM_AXES];
	struct joystick_button *buttons[JOYSTICK_NUM_BUTTONS];
};

static struct joystick *joystick_port[JOYSTICK_NUM_PORTS];

// Support the swap/cycle shortcuts:
static struct joystick_config *virtual_joystick_config;
static struct joystick *virtual_joystick = NULL;
static struct joystick_config *cycled_config[JOYSTICK_NUM_PORTS];

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
	new = g_malloc0(sizeof(struct joystick_config));
	new->index = num_configs;
	config_list = g_slist_append(config_list, new);
	num_configs++;
	return new;
}

unsigned joystick_config_count(void) {
	return num_configs;
}

struct joystick_config *joystick_config_index(unsigned i) {
	for (GSList *l = config_list; l; l = l->next) {
		struct joystick_config *jc = l->data;
		if (jc->index == i)
			return jc;
	}
	return NULL;
}

struct joystick_config *joystick_config_by_name(const char *name) {
	if (!name) return NULL;
	for (GSList *l = config_list; l; l = l->next) {
		struct joystick_config *jc = l->data;
		if (0 == strcmp(jc->name, name)) {
			return jc;
		}
	}
	return NULL;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static struct joystick_interface *find_if_in_mod(JoystickModule *module, const char *if_name) {
	if (!module || !if_name)
		return NULL;
	for (unsigned i = 0; module->interface_list[i]; i++) {
		if (0 == strcmp(module->interface_list[i]->name, if_name))
			return module->interface_list[i];
	}
	return NULL;
}

static struct joystick_interface *find_if_in_modlist(JoystickModule **list, const char *if_name) {
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
		JoystickModule *m = (JoystickModule *)module_select_by_arg((struct module **)ui_module->joystick_module_list, mod_name);
		if (!m) {
			m = (JoystickModule *)module_select_by_arg((struct module **)joystick_module_list, mod_name);
		}
		selected_interface = find_if_in_mod(m, if_name);
	} else if (if_name) {
		selected_interface = find_if(if_name);
	} else if (!selected_interface) {
		selected_interface = find_if("physical");
	}
}

void joystick_map(struct joystick_config *jc, unsigned port) {
	selected_interface = NULL;
	if (port >= JOYSTICK_NUM_PORTS)
		return;
	joystick_unmap(port);
	if (!jc)
		return;
	struct joystick *j = g_malloc0(sizeof(struct joystick));
	_Bool valid_joystick = 0;
	for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++) {
		char *spec_copy = g_strdup(jc->axis_specs[i]);
		char *spec = spec_copy;
		select_interface(&spec);
		if (!selected_interface) {
			g_free(spec_copy);
			return;
		}
		struct joystick_axis *axis = selected_interface->configure_axis(spec, i);
		j->axes[i] = axis;
		if (axis) {
			axis->interface = selected_interface;
			valid_joystick = 1;
		}
		g_free(spec_copy);
	}
	for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++) {
		char *spec_copy = g_strdup(jc->button_specs[i]);
		char *spec = spec_copy;
		select_interface(&spec);
		if (!selected_interface) {
			g_free(spec_copy);
			return;
		}
		struct joystick_button *button = selected_interface->configure_button(spec, i);
		j->buttons[i] = button;
		if (button) {
			button->interface = selected_interface;
			valid_joystick = 1;
		}
		g_free(spec_copy);
	}
	if (!valid_joystick) {
		g_free(j);
		return;
	}
	LOG_DEBUG(2, "Joystick port %d = %s [ ", port, jc->name);
	for (unsigned i = 0; i < JOYSTICK_NUM_AXES; i++) {
		if (j->axes[i])
			LOG_DEBUG(2, "%d=%s:", i, j->axes[i]->interface->name);
		LOG_DEBUG(2, ", ");
	}
	for (unsigned i = 0; i < JOYSTICK_NUM_BUTTONS; i++) {
		if (j->buttons[i])
			LOG_DEBUG(2, "%d=%s:", i, j->buttons[i]->interface->name);
		if ((i + 1) < JOYSTICK_NUM_BUTTONS)
			LOG_DEBUG(2, ", ");
	}
	LOG_DEBUG(2, " ]\n");
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
			struct joystick_interface *interface = axis->interface;
			if (interface->unmap_axis) {
				interface->unmap_axis(axis);
			} else {
				g_free(j->axes[a]);
			}
		}
	}
	for (unsigned b = 0; b < JOYSTICK_NUM_BUTTONS; b++) {
		struct joystick_button *button = j->buttons[b];
		if (button) {
			struct joystick_interface *interface = button->interface;
			if (interface->unmap_button) {
				interface->unmap_button(button);
			} else {
				g_free(j->buttons[b]);
			}
		}
	}
	g_free(j);
}

void joystick_set_virtual(struct joystick_config *jc) {
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
	struct joystick_config *tmp_jc = joystick_port_config[0];
	joystick_port_config[0] = joystick_port_config[1];
	joystick_port_config[1] = tmp_jc;
	struct joystick_config *tmp_cc = cycled_config[0];
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

void joystick_update(void) {
	unsigned port = (PIA0.b.control_register & 0x08) >> 3;
	unsigned axis = (PIA0.a.control_register & 0x08) >> 3;
	unsigned dac_value = PIA1.a.out_sink & 0xfc;
	unsigned js_value = 0;
	struct joystick *j = joystick_port[port];
	if (j && j->axes[axis]) {
		js_value = j->axes[axis]->read(j->axes[axis]->data);
	}
	if (js_value >= dac_value) {
		PIA0.a.in_sink |= 0x80;
	} else {
		PIA0.a.in_sink &= 0x7f;
	}
	if (joystick_port[0] && joystick_port[0]->buttons[0]) {
		if (joystick_port[0]->buttons[0]->read(joystick_port[0]->buttons[0]->data))
			PIA0.a.in_sink &= ~0x01;
	}
	if (joystick_port[1] && joystick_port[1]->buttons[0]) {
		if (joystick_port[1]->buttons[0]->read(joystick_port[1]->buttons[0]->data))
			PIA0.a.in_sink &= ~0x02;
	}
}
