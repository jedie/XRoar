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

#include <string.h>

#include "types.h"
#include "cart.h"
#include "logging.h"
#include "machine.h"
#include "m6809.h"
#include "mc6821.h"
#include "sam.h"
#include "xroar.h"

uint8_t *sam_mapped_rom;
static unsigned int map_type;
static unsigned int ram_page_bit;

static unsigned int sam_register;

static unsigned int sam_vdg_base;
static unsigned int sam_vdg_mode;
unsigned int sam_vdg_address;
static unsigned int sam_vdg_mod_xdiv;
static unsigned int sam_vdg_mod_ydiv;
unsigned int sam_vdg_mod_clear;
static unsigned int sam_vdg_xcount;
static unsigned int sam_vdg_ycount;
#ifdef VARIABLE_MPU_RATE
static unsigned int sam_ram_cycles;
static unsigned int sam_rom_cycles;
#else
# define sam_ram_cycles CPU_SLOW_DIVISOR
# define sam_rom_cycles CPU_SLOW_DIVISOR
#endif

static int cycles_remaining = 0;

static unsigned int vdg_mod_xdiv[8] = { 1, 3, 1, 2, 1, 1, 1, 1 };
static unsigned int vdg_mod_ydiv[8] = { 12, 1, 3, 1, 2, 1, 1, 1 };
static unsigned int vdg_mod_clear[8] = { ~30, ~14, ~30, ~14, ~30, ~14, ~30, ~0 };

/* SAM Data Sheet,
 *   Figure 6 - Signal routing for address multiplexer */
static unsigned int ram_row_masks[4] = { 0x007f, 0x007f, 0x00ff, 0x00ff };
static int ram_col_shifts[4] = { 2, 1, 0, 0 };
static unsigned int ram_col_masks[4] = { 0x3f00, 0x7f00, 0xff00, 0xff00 };
static unsigned int ram_row_mask;
static unsigned int ram_col_shift;
static unsigned int ram_col_mask;
static unsigned int ram_ras1;
#define VRAM_TRANSLATE(a) ( \
		((a << ram_col_shift) & ram_col_mask) \
		| (a & ram_row_mask) \
		| (!(a & 0x4000) ? ram_ras1 : 0) \
	)
#define RAM_TRANSLATE(a) (VRAM_TRANSLATE(a) | ram_page_bit)

static void update_from_register(void);

static inline void slow_cycle(int n) {
	int cycles = n * sam_ram_cycles;
	current_cycle += cycles;
	cycles_remaining -= cycles;
	if (cycles_remaining <= 0)
		m6809_running = 0;
}

#ifdef VARIABLE_MPU_RATE
static inline void fast_cycle(int n) {
	int cycles = n * sam_rom_cycles;
	current_cycle += cycles;
	cycles_remaining -= cycles;
	if (cycles_remaining <= 0)
		m6809_running = 0;
}
#else
# define fast_cycle(n) slow_cycle(n)
#endif

void sam_init(void) {
}

void sam_reset(void) {
	sam_set_register(0);
	sam_vdg_fsync();
}

void sam_run(int cycles) {
	cycles_remaining += cycles;
	m6809_running = 1;
	m6809_run();
}

unsigned int sam_read_byte(unsigned int addr) {
	static unsigned int last_read = 0;
	addr &= 0xffff;
	if (addr < 0x8000 || (addr >= 0xff00 && addr < 0xff20)) {
		slow_cycle(1);
	} else {
		fast_cycle(1);
	}
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	if (addr < 0x8000 || (map_type && addr < 0xff00)) {
		/* RAM access */
		unsigned int ram_addr = RAM_TRANSLATE(addr);
		if (addr < machine_ram_size)
			return last_read = ram0[ram_addr];
		return last_read;
	}
	if (addr < 0xc000) {
		/* BASIC ROM access */
		return last_read = sam_mapped_rom[addr & 0x3fff];
	}
	if (addr < 0xff00) {
		/* Cartridge ROM access */
		if (machine_cart) {
			return last_read = machine_cart->mem_data[addr & 0x3fff];
		}
		return last_read;
	}
	if (addr < 0xff20) {
		/* PIA0 */
		if (IS_COCO) {
			return last_read = mc6821_read(&PIA0, addr & 3);
		} else {
			if ((addr & 4) == 0) return last_read = mc6821_read(&PIA0, addr & 3);
			/* Not yet implemented:
			if ((addr & 7) == 4) return serial_stuff;
			if ((addr & 7) == 5) return serial_stuff;
			if ((addr & 7) == 6) return serial_stuff;
			if ((addr & 7) == 7) return serial_stuff;
			*/
		}
		return last_read;
	}
	if (addr < 0xff40) {
		return last_read = mc6821_read(&PIA1, addr & 3);
	}
	if (addr < 0xff60) {
		if (machine_cart && machine_cart->io_read) {
			return last_read = machine_cart->io_read(addr);
		}
		return last_read;
	}
	if (addr < 0xffe0)
		return last_read;
	return last_read = sam_mapped_rom[addr-0xc000];
}

void sam_store_byte(unsigned int addr, unsigned int octet) {
	addr &= 0xffff;
	if (addr < 0x8000 || (addr >= 0xff00 && addr < 0xff20)) {
		slow_cycle(1);
	} else {
		fast_cycle(1);
	}
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
	if (addr < 0x8000 || (map_type && addr < 0xff00)) {
		/* RAM access */
		unsigned int ram_addr = RAM_TRANSLATE(addr);
		if (IS_DRAGON32 && addr >= 0x8000 && machine_ram_size <= 0x8000) {
			ram_addr &= 0x7fff;
			/* TODO: Assuming cartridge ROM doesn't get selected
			 * here if present? */
			if (ram_addr < machine_ram_size && addr < 0xc000)
					ram0[ram_addr] = rom0[addr & 0x3fff];
			return;
		}
		if (addr < machine_ram_size)
			ram0[ram_addr] = octet;
		return;
	}
	if (addr < 0xc000) {
		/* BASIC ROM access */
		return;
	}
	if (addr < 0xff00) {
		/* Cartridge ROM access */
		if (machine_cart && machine_cart->mem_writable) {
			machine_cart->mem_data[addr & 0x3fff] = octet;
		}
		return;
	}
	if (addr < 0xff20) {
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
	if (addr < 0xff40) {
		mc6821_write(&PIA1, addr & 3, octet);
		return;
	}
	if (addr < 0xff60) {
		if (machine_cart && machine_cart->io_write) {
			machine_cart->io_write(addr, octet);
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
		update_from_register();
	}
}

void sam_nvma_cycles(int cycles) {
	slow_cycle(cycles);
	while (EVENT_PENDING(MACHINE_EVENT_LIST))
		DISPATCH_NEXT_EVENT(MACHINE_EVENT_LIST);
}

void sam_vdg_fsync(void) {
	sam_vdg_address = sam_vdg_base;
	sam_vdg_xcount = 0;
	sam_vdg_ycount = 0;
}

/* This routine copies bytes for the VDG.  Why so complex?  This implements the
 * divide-by-X and divide-by-Y parts of the SAM video address counter.  VDG
 * code will not only call this for video data, but at the end of each scanline
 * (with dest == NULL) to indicate the extra clocks a real VDG emits. */

void sam_vdg_bytes(int nbytes, uint8_t *dest) {
	unsigned int b15_5, b4, b3_0;
	b15_5 = sam_vdg_address & ~0x1f;
	b4 = sam_vdg_address & 0x10;
	b3_0 = sam_vdg_address & 0xf;
	while (nbytes > 0) {
		int n;
		if (b3_0 + nbytes >= 16) {
			n = 16 - b3_0;
		} else {
			n = nbytes;
		}
		if (dest) {
			uint8_t *src;
			/* In FAST mode, the VDG does not get access to RAM.
			 * Simulate by copying random data: */
			if (sam_ram_cycles == CPU_FAST_DIVISOR) {
				src = ram0;
			} else {
				src = ram0 + VRAM_TRANSLATE(sam_vdg_address);
			}
			memcpy(dest, src, n);
			dest += n;
		}
		b3_0 += n;
		nbytes -= n;
		if (b3_0 & 0x10) {
			b3_0 &= 0x0f;
			sam_vdg_xcount++;
			if (sam_vdg_xcount >= sam_vdg_mod_xdiv) {
				sam_vdg_xcount = 0;
				b4 += 0x10;
				if (b4 & 0x20) {
					b4 &= 0x10;
					sam_vdg_ycount++;
					if (sam_vdg_ycount >= sam_vdg_mod_ydiv) {
						sam_vdg_ycount = 0;
						b15_5 += 0x20;
						b15_5 &= 0xffff;
					}
				}
			}
		}
		sam_vdg_address = b15_5 | b4 | b3_0;
	}
}

void sam_set_register(unsigned int value) {
	sam_register = value;
	update_from_register();
}

unsigned int sam_get_register(void) {
	return sam_register;
}

static void update_from_register(void) {
	int memory_size = (sam_register >> 13) & 3;
	int mpu_rate = (sam_register >> 11) & 3;

	sam_vdg_mode = sam_register & 0x0007;
	sam_vdg_base = (sam_register & 0x03f8) << 6;
	sam_vdg_mod_xdiv = vdg_mod_xdiv[sam_vdg_mode];
	sam_vdg_mod_ydiv = vdg_mod_ydiv[sam_vdg_mode];
	sam_vdg_mod_clear = vdg_mod_clear[sam_vdg_mode];

	ram_row_mask = ram_row_masks[memory_size];
	ram_col_shift = ram_col_shifts[memory_size];
	ram_col_mask = ram_col_masks[memory_size];
	switch (memory_size) {
		case 0: /* 4K */
		case 1: /* 16K */
			ram_page_bit = 0;
			ram_ras1 = 0x8080;
			break;
		default: /* 64K */
			ram_page_bit = (sam_register & 0x0400) << 5;
			ram_ras1 = 0;
			break;
	}

	if ((map_type = sam_register & 0x8000)) {
		/* Map type 1 */
		if (mpu_rate == 1) {
			/* Disallow address-dependent MPU rate in map type 1 */
			mpu_rate = 0;
		}
	}

#ifdef VARIABLE_MPU_RATE
	switch (mpu_rate) {
		case 0:
			sam_ram_cycles = CPU_SLOW_DIVISOR;
			sam_rom_cycles = CPU_SLOW_DIVISOR;
			break;
		case 1:
			sam_ram_cycles = CPU_SLOW_DIVISOR;
			sam_rom_cycles = CPU_FAST_DIVISOR;
			break;
		default:
			sam_ram_cycles = CPU_FAST_DIVISOR;
			sam_rom_cycles = CPU_FAST_DIVISOR;
			break;
	}
#endif
}
