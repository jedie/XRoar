/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_CART_H_
#define XROAR_CART_H_

#include <inttypes.h>

struct machine_config;

enum cart_type {
	CART_ROM = 0,
	CART_DRAGONDOS = 1,
	CART_RSDOS = 2,
	CART_DELTADOS = 3,
};

struct cart_config {
	char *name;
	char *description;
	int index;
	enum cart_type type;
	char *rom;
	char *rom2;
	_Bool becker_port;
	int autorun;
};

struct cart {
	uint8_t mem_data[0x4000];
	int mem_writable;
	int mem_size;
	uint8_t (*io_read)(uint16_t A);
	void (*io_write)(uint16_t A, uint8_t D);
	void (*reset)(void);
	void (*attach)(void);
	void (*detach)(void);
};

struct cart_config *cart_config_new(void);
int cart_config_count(void);
struct cart_config *cart_config_index(int i);
struct cart_config *cart_config_by_name(const char *name);
struct cart_config *cart_find_working_dos(struct machine_config *mc);
void cart_config_complete(struct cart_config *cc);

void cart_configure(struct cart *c, struct cart_config *cc);

#endif  /* XROAR_CART_H_ */
