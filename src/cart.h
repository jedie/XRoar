/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_CART_H_
#define XROAR_CART_H_

#include <stdint.h>

#include "delegate.h"

struct machine_config;
struct event;

enum cart_type {
	CART_ROM = 0,
	CART_DRAGONDOS = 1,
	CART_RSDOS = 2,
	CART_DELTADOS = 3,
	CART_ORCH90 = 4,
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
	struct cart_config *config;
	void (*read)(struct cart *c, uint16_t A, _Bool P2, uint8_t *D);
	void (*write)(struct cart *c, uint16_t A, _Bool P2, uint8_t D);
	void (*reset)(struct cart *c);
	void (*attach)(struct cart *c);
	void (*detach)(struct cart *c);
	uint8_t *rom_data;
	DELEGATE_T1(void, bool) signal_firq;
	DELEGATE_T1(void, bool) signal_nmi;
	DELEGATE_T1(void, bool) signal_halt;
	struct event *firq_event;
};

struct cart_config *cart_config_new(void);
int cart_config_count(void);
struct cart_config *cart_config_index(int i);
struct cart_config *cart_config_by_name(const char *name);
struct cart_config *cart_find_working_dos(struct machine_config *mc);
void cart_config_complete(struct cart_config *cc);
void cart_config_print_all(void);

// c->config MUST point to a complete cart config before calling cart_init()
void cart_init(struct cart *c);
struct cart *cart_new(struct cart_config *cc);
struct cart *cart_new_named(const char *cc_name);
void cart_free(struct cart *c);

void cart_rom_init(struct cart *c);
struct cart *cart_rom_new(struct cart_config *cc);
void cart_rom_attach(struct cart *c);
void cart_rom_detach(struct cart *c);

#endif  /* XROAR_CART_H_ */
