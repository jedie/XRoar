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

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "breakpoint.h"
#include "list.h"
#include "m6809.h"
#include "misc.h"
#include "xroar.h"

enum bp_type {
	BP_INSTRUCTION,
};

struct breakpoint {
	int type;
	union {
		struct {
			int addr;
			void (*handler)(M6809State *);
		} instruction;
	} data;
};

static struct list *bp_instruction_list = NULL;

static void bp_instruction_hook(M6809State *cpu_state);

/**************************************************************************/

static struct breakpoint *bp_new(int type) {
	struct breakpoint *new = xmalloc(sizeof(struct breakpoint));
	memset(new, 0, sizeof(struct breakpoint));
	new->type = type;
	return new;
}

static void bp_add(struct breakpoint *bp) {
	switch (bp->type) {
	case BP_INSTRUCTION:
		bp_instruction_list = list_prepend(bp_instruction_list, bp);
		m6809_instruction_hook = bp_instruction_hook;
		break;
	default:
		break;
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

void bp_delete(struct breakpoint *bp) {
	bp_remove(bp);
	free(bp);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

struct breakpoint *bp_add_instr(int addr, void (*handler)(M6809State *)) {
	struct breakpoint *bp;
	if ((bp = bp_find_instr(addr, handler)))
		return bp;
	bp = bp_new(BP_INSTRUCTION);
	bp->data.instruction.addr = addr;
	bp->data.instruction.handler = handler;
	bp_add(bp);
	return bp;
}

struct breakpoint *bp_find_instr(int addr, void (*handler)(M6809State *)) {
	struct list *iter;
	for (iter = bp_instruction_list; iter; iter = iter->next) {
		struct breakpoint *bp = iter->data;
		if (bp->data.instruction.addr == addr
		    && bp->data.instruction.handler == handler) {
			return bp;
		}
	}
	return NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void bp_instruction_hook(M6809State *cpu_state) {
	struct list *iter;
	int pc = cpu_state->reg_pc;
	iter = bp_instruction_list;
	while (iter) {
		struct breakpoint *bp = iter->data;
		struct list *next = iter->next;
		if (pc == bp->data.instruction.addr) {
			bp->data.instruction.handler(cpu_state);
			if (pc != cpu_state->reg_pc) {
				/* pc changed, start again */
				next = bp_instruction_list;
				pc = cpu_state->reg_pc;
			}
		}
		iter = next;
	}
}
