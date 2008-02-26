/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "types.h"
#include "deltados.h"
#include "dragondos.h"
#include "joystick.h"
#include "keyboard.h"
#include "logging.h"
#include "machine.h"
#include "mc6821.h"
#include "rsdos.h"
#include "sam.h"
#include "tape.h"
#include "vdg.h"
#include "xroar.h"

uint8_t *addrptr_low;
uint8_t *addrptr_high;
static uint_least16_t map_type;

uint_least16_t sam_register;

uint_least16_t sam_vdg_base;
unsigned int  sam_vdg_mode;
uint_least16_t sam_vdg_address;
unsigned int  sam_vdg_mod_xdiv;
unsigned int  sam_vdg_mod_ydiv;
uint_least16_t sam_vdg_mod_clear;
unsigned int  sam_vdg_xcount;
unsigned int  sam_vdg_ycount;
#ifndef HAVE_GP32
unsigned int sam_topaddr_cycles;
#endif

static unsigned int vdg_mod_xdiv[8] = { 1, 3, 1, 2, 1, 1, 1, 1 };
static unsigned int vdg_mod_ydiv[8] = { 12, 1, 3, 1, 2, 1, 1, 1 };
static uint_least16_t vdg_mod_clear[8] = { ~30, ~14, ~30, ~14, ~30, ~14, ~30, ~0 };

void sam_init(void) {
}

void sam_reset(void) {
	sam_register = 0;
	sam_update_from_register();
	sam_vdg_fsync();
}

unsigned int sam_read_byte(uint_least16_t addr) {
	while (EVENT_PENDING(event_list))
		DISPATCH_NEXT_EVENT(event_list);
	addr &= 0xffff;
	if (addr < 0x8000) {
		current_cycle += CPU_SLOW_DIVISOR;
		if (addr < machine_page0_ram)
			return addrptr_low[addr];
		return 0x7e;
	}
	if (addr < 0xff00) {
		current_cycle += sam_topaddr_cycles;
		return addrptr_high[addr-0x8000];
	}
	if (addr < 0xff20) {
		current_cycle += CPU_SLOW_DIVISOR;
		if (IS_COCO) {
			return mc6821_read(&PIA0, addr & 3);
		} else {
			if ((addr & 4) == 0) return mc6821_read(&PIA0, addr & 3);
			/* Not yet implemented:
			if ((addr & 7) == 4) return serial_stuff;
			if ((addr & 7) == 5) return serial_stuff;
			if ((addr & 7) == 6) return serial_stuff;
			if ((addr & 7) == 7) return serial_stuff;
			*/
		}
		return 0x7e;
	}
	current_cycle += sam_topaddr_cycles;
	if (addr < 0xff40) {
		return mc6821_read(&PIA1, addr & 3);
	}
	if (addr < 0xff60) {
		if (!DOS_ENABLED)
			return 0x7e;
		if (IS_RSDOS) {
			/* CoCo floppy disk controller */
			return rsdos_read(addr);
		}
		if (IS_DRAGONDOS) {
			/* Dragon floppy disk controller */
			return dragondos_read(addr);
		}
		if (IS_DELTADOS) {
			/* Delta floppy disk controller */
			return deltados_read(addr);
		}
		return 0x7e;
	}
	if (addr < 0xffe0)
		return 0x7f;
	return rom0[addr-0xc000];
}

void sam_store_byte(uint_least16_t addr, unsigned int octet) {
	while (EVENT_PENDING(event_list))
		DISPATCH_NEXT_EVENT(event_list);
	addr &= 0xffff;
	if (addr < 0x8000) {
		current_cycle += CPU_SLOW_DIVISOR;
		if (addr < machine_page0_ram)
			addrptr_low[addr] = octet;
		return;
	}
	if (addr < 0xff00) {
		current_cycle += sam_topaddr_cycles;
		if (map_type) {
			if (IS_DRAGON32 && machine_page1_ram == 0) {
				ram0[addr-0x8000] = rom0[addr-0x8000];
				return;
			}
			if ((addr-0x8000) < machine_page1_ram) {
				ram1[addr-0x8000] = octet;
				return;
			}
		}
		return;
	}
	if (addr < 0xff20) {
		current_cycle += CPU_SLOW_DIVISOR;
		if (IS_COCO) {
			mc6821_write(&PIA0, addr & 3, octet);
		} else {
			if ((addr & 4) == 0) mc6821_write(&PIA0, addr & 3, octet);
			/* Not yet implemented:
			if ((addr & 7) == 4) serial_stuff;
			if ((addr & 7) == 5) serial_stuff;
			if ((addr & 7) == 6) serial_stuff;
			if ((addr & 7) == 7) serial_stuff;
			*/
		}
		return;
	}
	current_cycle += sam_topaddr_cycles;
	if (addr < 0xff40) {
		mc6821_write(&PIA1, addr & 3, octet);
		if ((addr & 3) == 2 && IS_DRAGON64 && !map_type) {
			/* Update ROM select on Dragon 64 */
			addrptr_high = (PIA1.b.port_output & 0x04) ? rom0 : rom1;
		}
		return;
	}
	if (addr < 0xff60) {
		if (!DOS_ENABLED)
			return;
		if (IS_RSDOS) {
			/* CoCo floppy disk controller */
			rsdos_write(addr, octet);
		}
		if (IS_DRAGONDOS) {
			/* Dragon floppy disk controller */
			dragondos_write(addr, octet);
		}
		if (IS_DELTADOS) {
			/* Delta floppy disk controller */
			deltados_write(addr, octet);
		}
		return;
	}
	if (addr < 0xffc0)
		return;
	if (addr < 0xffe0) {
		addr -= 0xffc0;
		if (addr & 1)
			sam_register |= 1 << (addr>>1);
		else
			sam_register &= ~(1 << (addr>>1));
		sam_update_from_register();
	}
}

void sam_update_from_register(void) {
	sam_vdg_mode = sam_register & 0x0007;
	sam_vdg_base = (sam_register & 0x03f8) << 6;
	sam_vdg_mod_xdiv = vdg_mod_xdiv[sam_vdg_mode];
	sam_vdg_mod_ydiv = vdg_mod_ydiv[sam_vdg_mode];
	sam_vdg_mod_clear = vdg_mod_clear[sam_vdg_mode];
	if ((map_type = sam_register & 0x8000)) {
		/* Map type 1 */
		addrptr_low = ram0;
		if (machine_page1_ram == 0)
			addrptr_high = ram0;
		else
			addrptr_high = ram1;
#ifndef HAVE_GP32
		sam_topaddr_cycles = CPU_SLOW_DIVISOR;
#endif
	} else {
		/* Map type 0 */
		if (sam_register & 0x0400) {
			/* Page #1 */
			addrptr_low = ram1;
		} else {
			/* Page #0 */
			addrptr_low = ram0;
		}
		if (IS_DRAGON64 && !(PIA1.b.port_output & 0x04))
			addrptr_high = rom1;
		else
			addrptr_high = rom0;
#ifndef HAVE_GP32
		sam_topaddr_cycles = (sam_register & 0x0800) ? CPU_FAST_DIVISOR : CPU_SLOW_DIVISOR;
#endif
	}
}
