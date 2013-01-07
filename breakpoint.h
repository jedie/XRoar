/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_BREAKPOINT_H_
#define XROAR_BREAKPOINT_H_

#include "mc6809.h"

enum bp_type {
	BP_INSTRUCTION = 0,  /* specifically 0 to be the C99 default */
};

/* flag conditions that must be met for breakpoints to be added to list: */
#define BP_MACHINE_ARCH (1 << 0)
#define BP_CRC_BAS (1 << 1)
#define BP_CRC_EXTBAS (1 << 2)
#define BP_CRC_ALTBAS (1 << 3)

/* flag conditions in which breakpoint then applies: */
#define BP_PAGE (1 << 4)
#define BP_MAP_TYPE (1 << 5)

struct breakpoint {
	enum bp_type type;
	unsigned int flags;
	void (*handler)(struct MC6809 *);
	uint16_t address;
	/* add conditions */
	int cond_machine_arch;
	const char *cond_crc_bas;
	const char *cond_crc_extbas;
	const char *cond_crc_altbas;
	/* active conditions */
	int cond_page;
	int cond_map_type;
};

/* convenience macros for standard types of breakpoint */

#define BP_DRAGON64_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_EXTBAS | BP_MAP_TYPE, .cond_crc_extbas = "@d64_1", .cond_map_type = 0, __VA_ARGS__ }
#define BP_DRAGON32_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_EXTBAS | BP_MAP_TYPE, .cond_crc_extbas = "@d32", .cond_map_type = 0, __VA_ARGS__ }
#define BP_DRAGON_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_EXTBAS | BP_MAP_TYPE, .cond_crc_extbas = "@dragon", .cond_map_type = 0, __VA_ARGS__ }

#define BP_COCO_BAS10_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_BAS | BP_MAP_TYPE, .cond_crc_bas = "@bas10", .cond_map_type = 0, __VA_ARGS__ }
#define BP_COCO_BAS11_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_BAS | BP_MAP_TYPE, .cond_crc_bas = "@bas11", .cond_map_type = 0, __VA_ARGS__ }
#define BP_COCO_BAS12_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_BAS | BP_MAP_TYPE, .cond_crc_bas = "@bas12", .cond_map_type = 0, __VA_ARGS__ }
#define BP_COCO_BAS13_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_BAS | BP_MAP_TYPE, .cond_crc_bas = "@bas13", .cond_map_type = 0, __VA_ARGS__ }
#define BP_COCO_ROM(...) \
	{ .type = BP_INSTRUCTION, .flags = BP_CRC_BAS | BP_MAP_TYPE, .cond_crc_bas = "@coco", .cond_map_type = 0, __VA_ARGS__ }

#define bp_add_list(bp) bp_add_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))
#define bp_remove_list(bp) bp_remove_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))

void bp_add(struct breakpoint *bp);
void bp_add_n(struct breakpoint *bp, int n);

void bp_remove(struct breakpoint *bp);
void bp_remove_n(struct breakpoint *bp, int n);

void bp_clear(void);

#endif  /* XROAR_BREAKPOINT_H_ */
