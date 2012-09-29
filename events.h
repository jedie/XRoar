/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_EVENT_H_
#define XROAR_EVENT_H_

typedef void (*event_delegate)(void *);

typedef unsigned int cycle_t;
extern cycle_t current_cycle;

typedef struct event_t event_t;
struct event_t {
	cycle_t at_cycle;
	event_delegate delegate;
	void *delegate_data;
	_Bool queued;
	event_t **list;
	event_t *next;
};

#define EVENT_EXISTS(list) (list)
#define EVENT_PENDING(list) (list && \
		(int)(current_cycle - list->at_cycle) >= 0)
#define DISPATCH_NEXT_EVENT(list) do { \
		event_t *e = list; \
		list = list->next; \
		e->queued = 0; \
		e->delegate(e->delegate_data); \
	} while (0)

event_t *event_new(event_delegate delegate, void *delegate_data);
void event_init(event_t *event, event_delegate delegate, void *delegate_data);

void event_free(event_t *event);
void event_queue(event_t **list, event_t *event);
void event_dequeue(event_t *event);

#endif  /* XROAR_EVENT_H_ */
