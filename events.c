/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "types.h"
#include "logging.h"

#include <stdlib.h>

#include "events.h"

event_t *event_list = NULL;

event_t *event_new(void) {
	event_t *new = malloc(sizeof(event_t));
	if (new == NULL)
		return NULL;
	new->at_cycle = 0;
	new->dispatch = NULL;
	new->context = NULL;
	new->queued = 0;
	new->next = NULL;
	return new;
}

void event_free(event_t *event) {
	event_dequeue(event);
	free(event);
}

void event_queue(event_t *event) {
	event_t **entry;
	if (event->queued)
		event_dequeue(event);
	event->queued = 1;
	for (entry = &event_list; *entry; entry = &((*entry)->next)) {
		if ((int)((*entry)->at_cycle - event->at_cycle) > 0) {
			event->next = *entry;
			*entry = event;
			return;
		}
	}
	*entry = event;
	event->next = NULL;
}

void event_dequeue(event_t *event) {
	event_t **entry = &event_list;
	event->queued = 0;
	if (*entry == event) {
		event_list = event->next;
		return;
	}
	for (entry = &event_list; *entry; entry = &((*entry)->next)) {
		if ((*entry)->next == event) {
			(*entry)->next = event->next;
			return;
		}
	}
}
