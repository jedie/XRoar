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

/* Sources:
 *     DragonDOS cartridge detail:
 *         http://www.dragon-archive.co.uk/
 */

/* TODO: I've hacked in an optional "becker port" at $FF49/$FF4A.  Is this the
 * best place for it? */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#include "becker.h"
#include "cart.h"
#include "delegate.h"
#include "dragondos.h"
#include "logging.h"
#include "machine.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

struct dragondos {
	struct cart cart;
	int ic1_old;
	int ic1_drive_select;
	_Bool ic1_motor_enable;
	_Bool ic1_precomp_enable;
	_Bool ic1_density;
	_Bool ic1_nmi_enable;
	_Bool have_becker;
	WD279X *fdc;
};

/* Handle signals from WD2797 */
static void set_drq(void *, _Bool);
static void set_intrq(void *, _Bool);

static void dragondos_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D);
static void dragondos_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D);
static void dragondos_reset(struct cart *c);
static void dragondos_detach(struct cart *c);
static void ff48_write(struct dragondos *d, int octet);

static void dragondos_init(struct dragondos *d) {
	struct cart *c = (struct cart *)d;
	struct cart_config *cc = c->config;
	cart_rom_init(c);
	c->read = dragondos_read;
	c->write = dragondos_write;
	c->reset = dragondos_reset;
	c->detach = dragondos_detach;
	d->have_becker = (cc->becker_port && becker_open());
	d->fdc = wd279x_new(WD2797);
	d->fdc->set_dirc = DELEGATE_AS1(void, int, vdrive_set_dirc, NULL);
	d->fdc->set_dden = DELEGATE_AS1(void, bool, vdrive_set_dden, NULL);
	d->fdc->set_sso = DELEGATE_AS1(void, unsigned, vdrive_set_sso, NULL);
	d->fdc->set_drq = DELEGATE_AS1(void, bool, set_drq, c);
	d->fdc->set_intrq = DELEGATE_AS1(void, bool, set_intrq, c);
}

struct cart *dragondos_new(struct cart_config *cc) {
	struct dragondos *d = xmalloc(sizeof(*d));
	d->cart.config = cc;
	dragondos_init(d);
	return (struct cart *)d;
}

static void dragondos_reset(struct cart *c) {
	struct dragondos *d = (struct dragondos *)c;
	wd279x_reset(d->fdc);
	d->ic1_old = 0xff;
	ff48_write(d, 0);
	if (d->have_becker)
		becker_reset();
}

static void dragondos_detach(struct cart *c) {
	struct dragondos *d = (struct dragondos *)c;
	wd279x_free(d->fdc);
	d->fdc = NULL;
	if (d->have_becker)
		becker_close();
	cart_rom_detach(c);
}

static void dragondos_read(struct cart *c, uint16_t A, _Bool P2, uint8_t *D) {
	struct dragondos *d = (struct dragondos *)c;
	if (!P2) {
		*D = c->rom_data[A & 0x3fff];
		return;
	}
	if ((A & 0xc) == 0) {
		*D = wd279x_read(d->fdc, A);
		return;
	}
	if (!(A & 8))
		return;
	if (d->have_becker) {
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

static void dragondos_write(struct cart *c, uint16_t A, _Bool P2, uint8_t D) {
	struct dragondos *d = (struct dragondos *)c;
	if (!P2)
		return;
	if ((A & 0xc) == 0) {
		wd279x_write(d->fdc, A, D);
		return;
	}
	if (!(A & 8))
		return;
	if (d->have_becker) {
		switch (A & 3) {
		case 0x0:
			ff48_write(d, D);
			break;
		case 0x2:
			becker_write_data(D);
			break;
		default:
			break;
		}
	} else {
		ff48_write(d, D);
	}
}

/* DragonDOS cartridge circuitry */
static void ff48_write(struct dragondos *d, int octet) {
	if (octet != d->ic1_old) {
		LOG_DEBUG(2, "DragonDOS: Write to FF48: ");
		if ((octet ^ d->ic1_old) & 0x03) {
			LOG_DEBUG(2, "DRIVE SELECT %01d, ", octet & 0x03);
		}
		if ((octet ^ d->ic1_old) & 0x04) {
			LOG_DEBUG(2, "MOTOR %s, ", (octet & 0x04)?"ON":"OFF");
		}
		if ((octet ^ d->ic1_old) & 0x08) {
			LOG_DEBUG(2, "DENSITY %s, ", (octet & 0x08)?"SINGLE":"DOUBLE");
		}
		if ((octet ^ d->ic1_old) & 0x10) {
			LOG_DEBUG(2, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
		}
		if ((octet ^ d->ic1_old) & 0x20) {
			LOG_DEBUG(2, "NMI %s, ", (octet & 0x20)?"ENABLED":"DISABLED");
		}
		LOG_DEBUG(2, "\n");
		d->ic1_old = octet;
	}
	d->ic1_drive_select = octet & 0x03;
	vdrive_set_drive(d->ic1_drive_select);
	d->ic1_motor_enable = octet & 0x04;
	d->ic1_density = octet & 0x08;
	wd279x_set_dden(d->fdc, !d->ic1_density);
	d->ic1_precomp_enable = octet & 0x10;
	d->ic1_nmi_enable = octet & 0x20;
}

static void set_drq(void *sptr, _Bool value) {
	struct cart *c = sptr;
	DELEGATE_CALL1(c->signal_firq, value);
}

static void set_intrq(void *sptr, _Bool value) {
	struct cart *c = sptr;
	struct dragondos *d = sptr;
	if (value) {
		if (d->ic1_nmi_enable) {
			DELEGATE_CALL1(c->signal_nmi, 1);
		}
	} else {
		DELEGATE_CALL1(c->signal_nmi, 0);
	}
}
