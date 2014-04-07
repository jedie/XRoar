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

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "slist.h"
#include "xalloc.h"

#include "cart.h"
#include "crc32.h"
#include "delegate.h"
#include "deltados.h"
#include "dragondos.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "orch90.h"
#include "romlist.h"
#include "rsdos.h"
#include "xroar.h"

static struct slist *config_list = NULL;
static int num_configs = 0;

/* Single config for auto-defined ROM carts */
static struct cart_config *rom_cart_config = NULL;

/* ---------------------------------------------------------------------- */

static void cart_rom_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D);
static void cart_rom_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D);
static void do_firq(void *);

/**************************************************************************/

struct cart_config *cart_config_new(void) {
	struct cart_config *new;
	new = xzalloc(sizeof(*new));
	new->index = num_configs;
	new->type = CART_ROM;
	new->autorun = ANY_AUTO;
	config_list = slist_append(config_list, new);
	num_configs++;
	return new;
}

int cart_config_count(void) {
	return num_configs;
}

struct cart_config *cart_config_index(int i) {
	for (struct slist *l = config_list; l; l = l->next) {
		struct cart_config *cc = l->data;
		if (cc->index == i)
			return cc;
	}
	return NULL;
}

struct cart_config *cart_config_by_name(const char *name) {
	if (!name) return NULL;
	for (struct slist *l = config_list; l; l = l->next) {
		struct cart_config *cc = l->data;
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
		char *tmp_name = alloca(strlen(name) + 1);
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
			rom_cart_config->description = xstrdup(bname);
		} else {
			rom_cart_config->description = xstrdup("ROM cartridge");
		}
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
		if (xroar_cfg.becker && (tmp = romlist_find("@rsdos_becker"))) {
			cc = cart_config_by_name("becker");
		} else if ((tmp = romlist_find("@rsdos"))) {
			cc = cart_config_by_name("rsdos");
		} else if (!xroar_cfg.becker && (tmp = romlist_find("@rsdos_becker"))) {
			cc = cart_config_by_name("becker");
		}
	}
	if (tmp)
		free(tmp);
	return cc;
}

void cart_config_complete(struct cart_config *cc) {
	if (!cc->description) {
		cc->description = xstrdup(cc->name);
	}
	if (cc->autorun == ANY_AUTO) {
		if (cc->type == CART_ROM) {
			cc->autorun = 1;
		} else {
			cc->autorun = 0;
		}
	}
}

static const char *cart_type_string[] = {
	"rom",
	"dragondos",
	"rsdos",
	"delta",
	"orch90",
};

void cart_config_print_all(void) {
	for (struct slist *l = config_list; l; l = l->next) {
		struct cart_config *cc = l->data;
		printf("cart %s\n", cc->name);
		if (cc->description) printf("  cart-desc %s\n", cc->description);
		if (cc->type >= 0 && cc->type < ARRAY_N_ELEMENTS(cart_type_string))
			printf("  cart-type %s\n", cart_type_string[cc->type]);
		if (cc->rom) printf("  cart-rom %s\n", cc->rom);
		if (cc->rom2) printf("  cart-rom2 %s\n", cc->rom2);
		if (cc->becker_port) printf("  cart-becker\n");
		if (cc->type != CART_ROM && cc->autorun) printf("  cart-autorun\n");
		if (cc->type == CART_ROM && !cc->autorun) printf("  no-cart-autorun\n");
		printf("\n");
	}
}

/* ---------------------------------------------------------------------- */

struct cart *cart_new(struct cart_config *cc) {
	if (!cc) return NULL;
	if (cc->description) {
		LOG_DEBUG(1, "Cartridge: %s\n", cc->description);
	}
	cart_config_complete(cc);
	struct cart *c;
	switch (cc->type) {
	default:
	case CART_ROM: c = cart_rom_new(cc); break;
	case CART_DRAGONDOS: c = dragondos_new(cc); break;
	case CART_RSDOS: c = rsdos_new(cc); break;
	case CART_DELTADOS: c = deltados_new(cc); break;
	case CART_ORCH90: c = orch90_new(cc); break;
	}
	if (c->attach)
		c->attach(c);
	return c;
}

struct cart *cart_new_named(const char *cc_name) {
	struct cart_config *cc = cart_config_by_name(cc_name);
	return cart_new(cc);
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
	c->rom_data = xzalloc(0x4000);
	if (cc->rom) {
		char *tmp = romlist_find(cc->rom);
		if (tmp) {
			int size = machine_load_rom(tmp, c->rom_data, 0x4000);
			if (size > 0) {
				uint32_t crc = crc32_block(CRC32_RESET, c->rom_data, size);
				LOG_DEBUG(1, "\tCRC = 0x%08x\n", crc);
			}
			free(tmp);
		}
	}
	if (cc->rom2) {
		char *tmp = romlist_find(cc->rom2);
		if (tmp) {
			int size = machine_load_rom(tmp, c->rom_data + 0x2000, 0x2000);
			if (size > 0) {
				uint32_t crc = crc32_block(CRC32_RESET, c->rom_data + 0x2000, size);
				LOG_DEBUG(1, "\tCRC = 0x%08x\n", crc);
			}
			free(tmp);
		}
	}
	c->signal_firq = DELEGATE_DEFAULT(bool);
	c->signal_nmi = DELEGATE_DEFAULT(bool);
	c->signal_halt = DELEGATE_DEFAULT(bool);
}

struct cart *cart_rom_new(struct cart_config *cc) {
	if (!cc) return NULL;
	struct cart *c = xmalloc(sizeof(*c));
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
		c->firq_event = event_new((delegate_null){do_firq, c});
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
	DELEGATE_SAFE_CALL1(c->signal_firq, 1);
	c->firq_event->at_tick = event_current_tick + (OSCILLATOR_RATE/10);
	event_queue(&MACHINE_EVENT_LIST, c->firq_event);
}
