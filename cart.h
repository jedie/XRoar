/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2009  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __CART_H__
#define __CART_H__

#include "types.h"

enum cart_type {
	CART_ROM,
	CART_RAM,
	CART_DRAGONDOS,
	CART_DELTADOS,
	CART_RSDOS,
};

struct cart {
	enum cart_type type;
	uint8_t mem_data[0x4000];
	int mem_writable;
	int (*io_read)(int addr);
	void (*io_write)(int addr, int value);
	void (*reset)(void);
	void (*detach)(void);
};

struct cart *cart_rom_new(const char *filename, int autorun);
struct cart *cart_ram_new(void);

#endif  /* __CART_H__ */
