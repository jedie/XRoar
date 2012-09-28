/*  Copyright 2003-2012 Ciaran Anscomb
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

#include <stdlib.h>
#include "portalib/glib.h"

#include "types.h"
#include "events.h"
#include "logging.h"

event_ticks event_current_tick = 0;

event_t *event_new(event_delegate delegate, void *delegate_data) {
	event_t *new = g_malloc(sizeof(event_t));
	event_init(new, delegate, delegate_data);
	return new;
}

void event_init(event_t *event, event_delegate delegate, void *delegate_data) {
	if (event == NULL) return;
	event->at_tick = event_current_tick;
	event->delegate = delegate;
	event->delegate_data = delegate_data;
	event->queued = 0;
	event->list = NULL;
	event->next = NULL;
}

void event_free(event_t *event) {
	event_dequeue(event);
	g_free(event);
}

void event_queue(event_t **list, event_t *event) {
	event_t **entry;
	if (event->queued)
		event_dequeue(event);
	event->list = list;
	event->queued = 1;
	for (entry = list; *entry; entry = &((*entry)->next)) {
		if ((int)((*entry)->at_tick - event->at_tick) > 0) {
			event->next = *entry;
			*entry = event;
			return;
		}
	}
	*entry = event;
	event->next = NULL;
}

void event_dequeue(event_t *event) {
	event_t **list = event->list;
	event_t **entry;
	event->queued = 0;
	if (list == NULL)
		return;
	if (*list == event) {
		*list = event->next;
		return;
	}
	for (entry = list; *entry; entry = &((*entry)->next)) {
		if ((*entry)->next == event) {
			(*entry)->next = event->next;
			return;
		}
	}
}
