/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_BREAKPOINT_H_
#define XROAR_BREAKPOINT_H_

#include "m6809.h"

enum bp_type {
	BP_INSTRUCTION = 0,  /* specifically 0 to be the C99 default */
};

/* flags determine conditions in which breakpoint applies */
#define BP_DRAGON (1 << 0)
#define BP_COCO (1 << 1)
#define BP_PAGE_0 (1 << 2)
#define BP_PAGE_1 (1 << 3)
#define BP_PAGE_ANY (BP_PAGE_0 | BP_PAGE_1)
#define BP_MAP_TYPE_0 (1 << 4)
#define BP_MAP_TYPE_1 (1 << 5)

#define BP_DRAGON_ROM (BP_DRAGON | BP_PAGE_ANY | BP_MAP_TYPE_0)
#define BP_COCO_ROM (BP_COCO | BP_PAGE_ANY | BP_MAP_TYPE_0)

struct breakpoint {
	enum bp_type type;
	unsigned int flags;
	uint16_t address;
	void (*handler)(M6809State *);
	/* If crc_nbytes > 0, breakpoint only added if CRC16 from given base
	 * address matches. */
	int crc_nbytes;
	uint16_t crc_address;
	uint16_t crc;
};

#define bp_add_list(bp) bp_add_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))
#define bp_remove_list(bp) bp_remove_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))

void bp_add(struct breakpoint *bp);
void bp_add_n(struct breakpoint *bp, int n);

void bp_remove(struct breakpoint *bp);
void bp_remove_n(struct breakpoint *bp, int n);

void bp_clear(void);

#endif  /* XROAR_BREAKPOINT_H_ */
