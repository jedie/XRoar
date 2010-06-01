/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_CART_H_
#define XROAR_CART_H_

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

#endif  /* XROAR_CART_H_ */
