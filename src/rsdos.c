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

/* Sources:
 *     CoCo DOS cartridge detail:
 *         http://www.coco3.com/unravalled/disk-basic-unravelled.pdf
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "becker.h"
#include "cart.h"
#include "logging.h"
#include "machine.h"
#include "rsdos.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

struct rsdos {
	struct cart cart;
	int ic1_old;
	int ic1_drive_select;
	_Bool ic1_density;
	_Bool drq_flag;
	_Bool intrq_flag;
	_Bool halt_enable;
	_Bool have_becker;
	WD279X *fdc;
};

static void rsdos_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D);
static void rsdos_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D);
static void rsdos_reset(struct cart *c);
static void rsdos_detach(struct cart *c);

/* Handle signals from WD2793 */
static void set_drq_handler(void *dptr);
static void reset_drq_handler(void *dptr);
static void set_intrq_handler(void *dptr);
static void reset_intrq_handler(void *dptr);

static void ff40_write(struct rsdos *r, int octet);

static void rsdos_init(struct rsdos *r) {
	struct cart *c = (struct cart *)r;
	struct cart_config *cc = c->config;
	cart_rom_init(c);
	c->read = rsdos_read;
	c->write = rsdos_write;
	c->reset = rsdos_reset;
	c->detach = rsdos_detach;
	r->have_becker = (cc->becker_port && becker_open());
	r->fdc = wd279x_new(WD2793);
	r->fdc->set_drq_handler = set_drq_handler;
	r->fdc->reset_drq_handler = reset_drq_handler;
	r->fdc->drq_data = c;
	r->fdc->set_intrq_handler = set_intrq_handler;
	r->fdc->reset_intrq_handler = reset_intrq_handler;
	r->fdc->intrq_data = c;
}

struct cart *rsdos_new(struct cart_config *cc) {
	struct rsdos *r = g_malloc(sizeof(*r));
	r->cart.config = cc;
	rsdos_init(r);
	return (struct cart *)r;
}

static void rsdos_reset(struct cart *c) {
	struct rsdos *r = (struct rsdos *)c;
	wd279x_reset(r->fdc);
	r->ic1_old = -1;
	r->ic1_drive_select = -1;
	r->drq_flag = r->intrq_flag = 0;
	ff40_write(r, 0);
	if (r->have_becker)
		becker_reset();
}

static void rsdos_detach(struct cart *c) {
	struct rsdos *r = (struct rsdos *)c;
	wd279x_free(r->fdc);
	r->fdc = NULL;
	if (r->have_becker)
		becker_close();
	cart_rom_detach(c);
}

static void rsdos_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D) {
	struct rsdos *r = (struct rsdos *)c;
	if (!P2) {
		*D = c->rom_data[A & 0x3fff];
		return;
	}
	if (A & 0x8) {
		*D = wd279x_read(r->fdc, A);
		return;
	}
	if (r->have_becker) {
		switch (A & 3) {
		case 0x1:
			*D = becker_read_status();
			break;
		case 0x2:
			*D = becker_read_data();
			break;
		default:
			break;
		}
	}
}

static void rsdos_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D) {
	struct rsdos *r = (struct rsdos *)c;
	if (!P2)
		return;
	if (A & 0x8) {
		wd279x_write(r->fdc, A, D);
		return;
	}
	if (r->have_becker) {
		/* XXX not exactly sure in what way anyone has tightened up the
		 * address decoding for the becker port */
		switch (A & 3) {
		case 0x0:
			ff40_write(r, D);
			break;
		case 0x2:
			becker_write_data(D);
			break;
		default:
			break;
		}
	} else {
		if (!(A & 8))
			ff40_write(r, D);
	}
}

/* RSDOS cartridge circuitry */
static void ff40_write(struct rsdos *r, int octet) {
	struct cart *c = (struct cart *)r;
	int new_drive_select = 0;
	octet ^= 0x20;
	if (octet & 0x01) {
		new_drive_select = 0;
	} else if (octet & 0x02) {
		new_drive_select = 1;
	} else if (octet & 0x04) {
		new_drive_select = 2;
	}
	vdrive_set_head(octet & 0x40 ? 1 : 0);
	if (octet != r->ic1_old) {
		LOG_DEBUG(4, "RSDOS: Write to FF40: ");
		if (new_drive_select != r->ic1_drive_select) {
			LOG_DEBUG(4, "DRIVE SELECT %d, ", new_drive_select);
		}
		if ((octet ^ r->ic1_old) & 0x08) {
			LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x08)?"ON":"OFF");
		}
		if ((octet ^ r->ic1_old) & 0x20) {
			LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x20)?"SINGLE":"DOUBLE");
		}
		if ((octet ^ r->ic1_old) & 0x10) {
			LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
		}
		if ((octet ^ r->ic1_old) & 0x40) {
			LOG_DEBUG(4, "SIDE %d, ", (octet & 0x40) >> 6);
		}
		if ((octet ^ r->ic1_old) & 0x80) {
			LOG_DEBUG(4, "HALT %s, ", (octet & 0x80)?"ENABLED":"DISABLED");
		}
		LOG_DEBUG(4, "\n");
		r->ic1_old = octet;
	}
	r->ic1_drive_select = new_drive_select;
	vdrive_set_drive(r->ic1_drive_select);
	r->ic1_density = octet & 0x20;
	wd279x_set_dden(r->fdc, !r->ic1_density);
	if (r->ic1_density && r->intrq_flag) {
		if (c->signal_nmi.delegate)
			c->signal_nmi.delegate(c->signal_nmi.dptr, 1);
	}
	r->halt_enable = octet & 0x80;
	if (r->intrq_flag) r->halt_enable = 0;
	if (c->signal_halt.delegate)
		c->signal_halt.delegate(c->signal_halt.dptr, r->halt_enable && !r->drq_flag);
}

static void set_drq_handler(void *dptr) {
	struct cart *c = dptr;
	struct rsdos *r = dptr;
	r->drq_flag = 1;
	if (c->signal_halt.delegate)
		c->signal_halt.delegate(c->signal_halt.dptr, 0);
}

static void reset_drq_handler(void *dptr) {
	struct cart *c = dptr;
	struct rsdos *r = dptr;
	r->drq_flag = 0;
	if (r->halt_enable) {
		if (c->signal_halt.delegate)
			c->signal_halt.delegate(c->signal_halt.dptr, 1);
	}
}

static void set_intrq_handler(void *dptr) {
	struct cart *c = dptr;
	struct rsdos *r = dptr;
	r->intrq_flag = 1;
	r->halt_enable = 0;
	if (c->signal_halt.delegate)
		c->signal_halt.delegate(c->signal_halt.dptr, 0);
	if (!r->ic1_density && r->intrq_flag) {
		if (c->signal_nmi.delegate)
			c->signal_nmi.delegate(c->signal_nmi.dptr, 1);
	}
}

static void reset_intrq_handler(void *dptr) {
	struct cart *c = dptr;
	struct rsdos *r = dptr;
	r->intrq_flag = 0;
	if (c->signal_nmi.delegate)
		c->signal_nmi.delegate(c->signal_nmi.dptr, 0);
}
