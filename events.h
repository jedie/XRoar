/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
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

typedef struct event_t event_t;
struct event_t {
	event_ticks at_tick;
	event_delegate delegate;
	void *delegate_data;
	_Bool queued;
	event_t **list;
	event_t *next;
};

#define event_exists(list) (list)

#define event_pending(list) (list && \
		(int)(event_current_tick - list->at_tick) >= 0)

#define event_dispatch_next(list) do { \
		event_t *e = list; \
		list = list->next; \
		e->queued = 0; \
		e->delegate(e->delegate_data); \
	} while (0)

#define event_run_queue(list) do { \
		while (event_pending(list)) \
			event_dispatch_next(list); \
	} while (0)

event_t *event_new(event_delegate delegate, void *delegate_data);
void event_init(event_t *event, event_delegate delegate, void *delegate_data);

void event_free(event_t *event);
void event_queue(event_t **list, event_t *event);
void event_dequeue(event_t *event);

#endif  /* XROAR_EVENT_H_ */
