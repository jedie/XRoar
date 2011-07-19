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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "types.h"

#include "breakpoint.h"
#include "list.h"
#include "m6809.h"
#include "machine.h"
#include "misc.h"
#include "sam.h"
#include "xroar.h"

static struct list *bp_instruction_list = NULL;

static void bp_instruction_hook(M6809State *cpu_state);

/**************************************************************************/

void bp_add(struct breakpoint *bp) {
	switch (bp->type) {
	case BP_INSTRUCTION:
		bp_instruction_list = list_prepend(bp_instruction_list, bp);
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
		bp_instruction_list = list_delete(bp_instruction_list, bp);
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
	struct list *iter;
	int pc = cpu_state->reg_pc;
	int flags = IS_DRAGON ? BP_DRAGON : BP_COCO;
	unsigned int sam_register = sam_get_register();
	flags |= (sam_register & 0x0400) ? BP_PAGE_1 : BP_PAGE_0;
	flags |= (sam_register & 0x8000) ? BP_MAP_TYPE_1 : BP_MAP_TYPE_0;
	iter = bp_instruction_list;
	while (iter) {
		struct breakpoint *bp = iter->data;
		struct list *next = iter->next;
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
