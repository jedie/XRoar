/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_EVENT_H_
#define XROAR_EVENT_H_

/* Maintains queues of events.  Each event has a tick number at which its
 * delegate is scheduled to run.  */

typedef unsigned int event_ticks;

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

#define event_exists(list) (list)

#define event_pending(list) (list && \
		(int)(event_current_tick - list->at_tick) >= 0)

#define event_dispatch_next(list) do { \
		struct event *e = list; \
		list = list->next; \
		e->queued = 0; \
		e->delegate(e->delegate_data); \
	} while (0)

#define event_run_queue(list) do { \
		while (event_pending(list)) \
			event_dispatch_next(list); \
	} while (0)

struct event *event_new(event_delegate delegate, void *delegate_data);
void event_init(struct event *event, event_delegate delegate, void *delegate_data);

void event_free(struct event *event);
void event_queue(struct event **list, struct event *event);
void event_dequeue(struct event *event);

#endif  /* XROAR_EVENT_H_ */
