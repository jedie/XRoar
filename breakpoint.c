/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "portalib/glib.h"

#include "breakpoint.h"
#include "crclist.h"
#include "machine.h"
#include "mc6809.h"
#include "sam.h"
#include "xroar.h"

static GSList *bp_instruction_list = NULL;

static void bp_instruction_hook(struct MC6809 *cpu);

/**************************************************************************/

void bp_add(struct breakpoint *bp) {
	if ((bp->flags & BP_MACHINE_ARCH) && xroar_machine_config->architecture != bp->cond_machine_arch)
		return;
	if ((bp->flags & BP_CRC_BAS) && (!has_bas || !crclist_match(bp->cond_crc_bas, crc_bas)))
		return;
	if ((bp->flags & BP_CRC_EXTBAS) && (!has_extbas || !crclist_match(bp->cond_crc_extbas, crc_extbas)))
		return;
	if ((bp->flags & BP_CRC_ALTBAS) && (!has_altbas || !crclist_match(bp->cond_crc_altbas, crc_altbas)))
		return;
	switch (bp->type) {
	case BP_INSTRUCTION:
		bp_instruction_list = g_slist_prepend(bp_instruction_list, bp);
		CPU0->instruction_hook = bp_instruction_hook;
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
			CPU0->instruction_hook = NULL;
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

static void bp_instruction_hook(struct MC6809 *cpu) {
	GSList *iter = bp_instruction_list;
	uint16_t pc = CPU0->reg_pc;
	unsigned int sam_register = sam_get_register();
	int page = (sam_register & 0x0400) ? 1 : 0;
	int map_type = (sam_register & 0x8000) ? 1 : 0;
	while (iter) {
		struct breakpoint *bp = iter->data;
		iter = iter->next;
		if (bp->type != BP_INSTRUCTION)
			continue;
		if ((bp->flags & BP_PAGE) && page != bp->cond_page)
			continue;
		if ((bp->flags & BP_MAP_TYPE) && map_type != bp->cond_map_type)
			continue;
		if (pc != bp->address)
			continue;
		bp->handler(cpu);
		if (pc != CPU0->reg_pc) {
			/* pc changed, start again */
			iter = bp_instruction_list;
			pc = CPU0->reg_pc;
		}
	}
}
