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

#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "portalib/glib.h"

#include "becker.h"
#include "cart.h"
#include "deltados.h"
#include "dragondos.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "romlist.h"
#include "rsdos.h"
#include "xroar.h"

struct cart_config_template {
	const char *name;
	const char *description;
	int type;
	const char *rom;
	_Bool becker_port;
};

static struct cart_config_template config_templates[] = {
	{ .name = "dragondos", .description = "DragonDOS", .type = CART_DRAGONDOS, .rom = "@dragondos_compat" },
	{ .name = "rsdos", .description = "RS-DOS", .type = CART_RSDOS, .rom = "@rsdos" },
	{ .name = "delta", .description = "Delta System", .type = CART_DELTADOS, .rom = "@delta" },
	{ .name = "becker", .description = "RS-DOS with becker port", .type = CART_RSDOS, .rom = "@rsdos_becker", .becker_port = 1 },
};
#define NUM_CONFIG_TEMPLATES (int)(sizeof(config_templates)/sizeof(struct cart_config_template))

static struct cart_config **configs = NULL;
static int num_configs = NUM_CONFIG_TEMPLATES;

/* Single config for auto-defined ROM carts */
static struct cart_config *rom_cart_config = NULL;

/* ---------------------------------------------------------------------- */

static void cart_rom_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D);
static void cart_rom_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D);
static void do_firq(void *);

/**************************************************************************/

/* Create config array */
static int alloc_config_array(int size) {
	struct cart_config **new_list;
	int clear_from = num_configs;
	if (!configs) clear_from = 0;
	new_list = g_realloc(configs, size * sizeof(struct cart_config *));
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
	configs[i] = g_malloc0(sizeof(struct cart_config));
	configs[i]->name = g_strdup(config_templates[i].name);
	configs[i]->description = g_strdup(config_templates[i].description);
	configs[i]->type = config_templates[i].type;
	configs[i]->rom = g_strdup(config_templates[i].rom);
	configs[i]->becker_port = config_templates[i].becker_port;
	configs[i]->index = i;
	return 0;
}

struct cart_config *cart_config_new(void) {
	struct cart_config *new;
	if (alloc_config_array(num_configs+1) != 0)
		return NULL;
	new = g_malloc0(sizeof(struct cart_config));
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
			rom_cart_config->name = g_strdup("romcart");
		}
		if (rom_cart_config->description) {
			g_free(rom_cart_config->description);
		}
		/* Make up a description from filename */
		char *tmp_name = g_alloca(strlen(name) + 1);
		strcpy(tmp_name, name);
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
			rom_cart_config->description = g_strdup(bname);
		} else {
			rom_cart_config->description = g_strdup("ROM cartridge");
		}
		rom_cart_config->type = CART_ROM;
		if (rom_cart_config->rom) g_free(rom_cart_config->rom);
		rom_cart_config->rom = g_strdup(name);
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
		if (xroar_opt_becker && (tmp = romlist_find("@rsdos_becker"))) {
			cc = cart_config_by_name("becker");
		} else if ((tmp = romlist_find("@rsdos"))) {
			cc = cart_config_by_name("rsdos");
		} else if (!xroar_opt_becker && (tmp = romlist_find("@rsdos_becker"))) {
			cc = cart_config_by_name("becker");
		}
	}
	if (tmp)
		g_free(tmp);
	return cc;
}

void cart_config_complete(struct cart_config *cc) {
	if (!cc->description) {
		cc->description = g_strdup(cc->name);
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

struct cart *cart_new(struct cart_config *cc) {
	if (!cc) return NULL;
	if (cc->description) {
		LOG_DEBUG(2, "Cartridge: %s\n", cc->description);
	}
	cart_config_complete(cc);
	struct cart *c;
	switch (cc->type) {
	default:
	case CART_ROM: c = cart_rom_new(cc); break;
	case CART_DRAGONDOS: c = dragondos_new(cc); break;
	case CART_RSDOS: c = rsdos_new(cc); break;
	case CART_DELTADOS: c = deltados_new(cc); break;
	}
	if (c->attach)
		c->attach(c);
	return c;
}

void cart_free(struct cart *c) {
	if (!c) return;
	if (c->detach)
		c->detach(c);
	free(c);
}

/* ROM cart routines */

static void dummy_cart(struct cart *c) { (void)c; }

void cart_rom_init(struct cart *c) {
	struct cart_config *cc = c->config;
	assert(cc != NULL);
	c->read = cart_rom_read;
	c->write = cart_rom_write;
	c->reset = dummy_cart;
	c->attach = dummy_cart;
	c->detach = cart_rom_detach;
	c->attach = cart_rom_attach;
	c->rom_data = g_malloc0(0x4000);
	if (cc->rom) {
		char *tmp = romlist_find(cc->rom);
		if (tmp) {
			machine_load_rom(tmp, c->rom_data, 0x4000);
			g_free(tmp);
		}
	}
	if (cc->rom2) {
		char *tmp = romlist_find(cc->rom2);
		if (tmp) {
			machine_load_rom(tmp, c->rom_data + 0x2000, 0x2000);
			g_free(tmp);
		}
	}
}

struct cart *cart_rom_new(struct cart_config *cc) {
	if (!cc) return NULL;
	struct cart *c = g_malloc(sizeof(struct cart));
	c->config = cc;
	cart_rom_init(c);
	return c;
}

static void cart_rom_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D) {
	if (!P2)
		*D = c->rom_data[A & 0x3fff];
}

static void cart_rom_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D) {
	(void)c;
	(void)A;
	(void)P2;
	(void)D;
}

void cart_rom_attach(struct cart *c) {
	struct cart_config *cc = c->config;
	if (cc->autorun) {
		c->firq_event = event_new(do_firq, c);
		c->firq_event->at_tick = event_current_tick + (OSCILLATOR_RATE/10);
		event_queue(&MACHINE_EVENT_LIST, c->firq_event);
	} else {
		c->firq_event = NULL;
	}
}

void cart_rom_detach(struct cart *c) {
	if (c->firq_event) {
		event_dequeue(c->firq_event);
		event_free(c->firq_event);
		c->firq_event = NULL;
	}
	if (c->rom_data) {
		free(c->rom_data);
		c->rom_data = NULL;
	}
}

static void do_firq(void *data) {
	struct cart *c = data;
	PIA_SET_Cx1(PIA1.b);
	c->firq_event->at_tick = event_current_tick + (OSCILLATOR_RATE/10);
	event_queue(&MACHINE_EVENT_LIST, c->firq_event);
}
