/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "cart.h"
#include "deltados.h"
#include "dragondos.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "misc.h"
#include "romlist.h"
#include "rsdos.h"
#include "xroar.h"

struct cart_config_template {
	const char *name;
	const char *description;
	int type;
	const char *rom;
};

static struct cart_config_template config_templates[] = {
	{ "dragondos", "DragonDOS", CART_DRAGONDOS, "@dragondos_compat" },
	{ "rsdos", "RS-DOS", CART_RSDOS, "@rsdos" },
	{ "delta", "Delta System", CART_DELTADOS, "@delta" },
};
#define NUM_CONFIG_TEMPLATES (int)(sizeof(config_templates)/sizeof(struct cart_config_template))

static struct cart_config **configs = NULL;
static int num_configs = NUM_CONFIG_TEMPLATES;

/* Single config for auto-defined ROM carts */
static struct cart_config *rom_cart_config = NULL;

/* ---------------------------------------------------------------------- */

static void rom_configure(struct cart *c, struct cart_config *cc);

static event_t *firq_event;
static void do_firq(void);

/**************************************************************************/

/* Create config array */
static int alloc_config_array(int size) {
	struct cart_config **new_list;
	int clear_from = num_configs;
	if (!configs) clear_from = 0;
	new_list = xrealloc(configs, size * sizeof(struct cart_config *));
	configs = new_list;
	memset(&configs[clear_from], 0, (size - clear_from) * sizeof(struct cart_config *));
	return 0;
}

/* Populate config from template */
static int populate_config_index(int i) {
	assert(configs != NULL);
	assert(i >= 0 && i < NUM_CONFIG_TEMPLATES);
	if (configs[i])
		return 0;
	configs[i] = xmalloc(sizeof(struct cart_config));
	memset(configs[i], 0, sizeof(struct cart_config));
	configs[i]->name = xstrdup(config_templates[i].name);
	configs[i]->description = strdup(config_templates[i].description);
	configs[i]->type = config_templates[i].type;
	configs[i]->rom = xstrdup(config_templates[i].rom);
	configs[i]->index = i;
	return 0;
}

struct cart_config *cart_config_new(void) {
	struct cart_config *new;
	if (alloc_config_array(num_configs+1) != 0)
		return NULL;
	new = xmalloc(sizeof(struct cart_config));
	memset(new, 0, sizeof(struct cart_config));
	new->index = num_configs;
	new->type = CART_ROM;
	new->autorun = ANY_AUTO;
	configs[num_configs++] = new;
	return new;
}

int cart_config_count(void) {
	return num_configs;
}

struct cart_config *cart_config_index(int i) {
	if (i < 0 || i >= num_configs) {
		return NULL;
	}
	if (!configs) {
		if (alloc_config_array(num_configs) != 0)
			return NULL;
	}
	if (i < NUM_CONFIG_TEMPLATES && !configs[i]) {
		if (populate_config_index(i) != 0)
			return NULL;
	}
	return configs[i];
}

struct cart_config *cart_config_by_name(const char *name) {
	int count, i;
	if (!name) return NULL;
	count = cart_config_count();
	for (i = 0; i < count; i++) {
		struct cart_config *cc = cart_config_index(i);
		if (0 == strcmp(cc->name, name)) {
			return cc;
		}
	}
	/* If "name" turns out to be a loadable ROM file, create a special
	   ROM cart config for it. */
	if (xroar_filetype_by_ext(name) == FILETYPE_ROM) {
		if (!rom_cart_config) {
			if (!(rom_cart_config = cart_config_new())) {
				return NULL;
			}
			rom_cart_config->name = xstrdup("romcart");
		}
		if (rom_cart_config->description) {
			free(rom_cart_config->description);
		}
		/* Make up a description from filename */
		char *tmp_name = xstrdup(name);
		char *bname = basename(tmp_name);
		if (bname && *bname) {
			char *sep;
			/* this will strip off file extensions or TOSEC-style
			   metadata in brackets */
			for (sep = bname + 1; *sep; sep++) {
				if ((*sep == '(') ||
				    (*sep == '.') ||
				    (isspace((int)*sep) && *(sep+1) == '(')) {
					*sep = 0;
					break;
				}
			}
			rom_cart_config->description = strdup(bname);
		} else {
			rom_cart_config->description = strdup("ROM cartridge");
		}
		free(tmp_name);
		rom_cart_config->type = CART_ROM;
		if (rom_cart_config->rom) free(rom_cart_config->rom);
		rom_cart_config->rom = xstrdup(name);
		rom_cart_config->autorun = 1;
		return rom_cart_config;
	}
	return NULL;
}

struct cart_config *cart_find_working_dos(struct machine_config *mc) {
	char *tmp = NULL;
	struct cart_config *cc = NULL;
	if (!mc || mc->architecture != ARCH_COCO) {
		if ((tmp = romlist_find("@dragondos_compat"))) {
			cc = cart_config_by_name("dragondos");
		} else if ((tmp = romlist_find("@delta"))) {
			cc = cart_config_by_name("delta");
		}
	} else {
		if ((tmp = romlist_find("@rsdos"))) {
			cc = cart_config_by_name("rsdos");
		}
	}
	if (tmp)
		free(tmp);
	return cc;
}

void cart_config_complete(struct cart_config *cc) {
	if (!cc->description) {
		cc->description = strdup(cc->name);
	}
	if (cc->autorun == ANY_AUTO) {
		if (cc->type == CART_ROM) {
			cc->autorun = 1;
		} else {
			cc->autorun = 0;
		}
	}
}

/* ---------------------------------------------------------------------- */

void cart_configure(struct cart *c, struct cart_config *cc) {
	if (!c || !cc) return;
	if (cc->description) {
		LOG_DEBUG(2, "Cartridge: %s\n", cc->description);
	}
	cart_config_complete(cc);
	/* */
	memset(c->mem_data, 0, sizeof(c->mem_data));
	if (cc->rom) {
		char *tmp = romlist_find(cc->rom);
		if (tmp) {
			machine_load_rom(tmp, c->mem_data, sizeof(c->mem_data));
			free(tmp);
		}
	}
	if (cc->rom2) {
		char *tmp = romlist_find(cc->rom2);
		if (tmp) {
			machine_load_rom(tmp, c->mem_data + 0x2000, sizeof(c->mem_data) - 0x2000);
			free(tmp);
		}
	}
	switch (cc->type) {
		default:
		case CART_ROM:       rom_configure(c, cc); break;
		case CART_DRAGONDOS: dragondos_configure(c, cc); break;
		case CART_RSDOS:     rsdos_configure(c, cc); break;
		case CART_DELTADOS:  deltados_configure(c, cc); break;
	}
}

/* Routines specific to ROM carts */

static void attach_rom(void) {
	firq_event = event_new();
	firq_event->dispatch = do_firq;
	firq_event->at_cycle = current_cycle + (OSCILLATOR_RATE/10);
	event_queue(&MACHINE_EVENT_LIST, firq_event);
}

static void detach_rom(void) {
	if (firq_event) {
		event_dequeue(firq_event);
		event_free(firq_event);
		firq_event = NULL;
	}
}

static void rom_configure(struct cart *c, struct cart_config *cc) {
	if (cc->autorun) {
		c->attach = attach_rom;
		c->detach = detach_rom;
	}
}

static void do_firq(void) {
	PIA_SET_Cx1(PIA1.b);
	firq_event->at_cycle = current_cycle + (OSCILLATOR_RATE/10);
	event_queue(&MACHINE_EVENT_LIST, firq_event);
}
