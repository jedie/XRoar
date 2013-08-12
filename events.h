/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_EVENT_H_
#define XROAR_EVENT_H_

#include <limits.h>

/* Maintains queues of events.  Each event has a tick number at which its
 * delegate is scheduled to run.  */

typedef unsigned event_ticks;

/* Current "time". */
extern event_ticks event_current_tick;

typedef void (*event_delegate)(void *);

struct event {
	event_ticks at_tick;
	event_delegate delegate;
	void *delegate_data;
	_Bool queued;
	struct event **list;
	struct event *next;
};

struct event *event_new(event_delegate delegate, void *delegate_data);
void event_init(struct event *event, event_delegate delegate, void *delegate_data);

void event_free(struct event *event);
void event_queue(struct event **list, struct event *event);
void event_dequeue(struct event *event);

static inline _Bool event_pending(struct event **list) {
	return *list && (event_current_tick - (*list)->at_tick) <= (UINT_MAX/2);
}

static inline void event_dispatch_next(struct event **list) {
	struct event *e = *list;
	*list = e->next;
	e->queued = 0;
	e->delegate(e->delegate_data);
}

static inline void event_run_queue(struct event **list) {
	while (event_pending(list))
		event_dispatch_next(list);
}

#endif  /* XROAR_EVENT_H_ */
