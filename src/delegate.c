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

#include <stdlib.h>

#include "delegate.h"

void delegate_default_null(void *sptr) {
	(void)sptr;
}

void delegate_default_bool(void *sptr, _Bool value) {
	(void)sptr;
	(void)value;
}

void delegate_default_int(void *sptr, int value) {
	(void)sptr;
	(void)value;
}

void delegate_default_unsigned(void *sptr, unsigned value) {
	(void)sptr;
	(void)value;
}

void delegate_default_float(void *sptr, float value) {
	(void)sptr;
	(void)value;
}
