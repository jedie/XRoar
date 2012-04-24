/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"

#include "types.h"
#include "breakpoint.h"
#include "crc16.h"
#include "m6809.h"
#include "machine.h"
#include "sam.h"
#include "xroar.h"

static GSList *bp_instruction_list = NULL;

static void bp_instruction_hook(M6809State *cpu_state);

/**************************************************************************/

void bp_add(struct breakpoint *bp) {
	unsigned int flags = IS_DRAGON ? BP_DRAGON : BP_COCO;
	if ((bp->flags & flags) != flags)
		return;
	if (bp->crc_nbytes > 0) {
		uint16_t crc = crc16_block(CRC16_RESET, &machine_rom[bp->crc_address & 0x3fff], bp->crc_nbytes);
		if (crc != bp->crc) {
			return;
		}
	}
	switch (bp->type) {
	case BP_INSTRUCTION:
		bp_instruction_list = g_slist_prepend(bp_instruction_list, bp);
		m6809_instruction_hook = bp_instruction_hook;
		break;
	default:
		break;
	}
}

void bp_add_n(struct breakpoint *bp, int n) {
	int i;
	for (i = 0; i < n; i++) {
		bp_add(&bp[i]);
	}
}

void bp_remove(struct breakpoint *bp) {
	switch (bp->type) {
	case BP_INSTRUCTION:
		bp_instruction_list = g_slist_remove(bp_instruction_list, bp);
		if (!bp_instruction_list) {
			m6809_instruction_hook = NULL;
		}
		break;
	default:
		break;
	}
}

void bp_remove_n(struct breakpoint *bp, int n) {
	int i;
	for (i = 0; i < n; i++) {
		bp_remove(&bp[i]);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void bp_instruction_hook(M6809State *cpu_state) {
	GSList *iter;
	uint16_t pc = cpu_state->reg_pc;
	unsigned int sam_register = sam_get_register();
	unsigned int flags = (sam_register & 0x0400) ? BP_PAGE_1 : BP_PAGE_0;
	flags |= (sam_register & 0x8000) ? BP_MAP_TYPE_1 : BP_MAP_TYPE_0;
	iter = bp_instruction_list;
	while (iter) {
		struct breakpoint *bp = iter->data;
		GSList *next = iter->next;
		if ((bp->flags & flags) == flags && pc == bp->address) {
			bp->handler(cpu_state);
			if (pc != cpu_state->reg_pc) {
				/* pc changed, start again */
				next = bp_instruction_list;
				pc = cpu_state->reg_pc;
			}
		}
		iter = next;
	}
}
