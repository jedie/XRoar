/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_EVENT_H_
#define XROAR_EVENT_H_

typedef struct event_t event_t;
struct event_t {
	Cycle at_cycle;
	void (*dispatch)(void);
	int queued;
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
		e->dispatch(); \
	} while (0)

event_t *event_new(void);
void event_init(event_t *event);  /* for static declarations */

void event_free(event_t *event);
void event_queue(event_t **list, event_t *event);
void event_dequeue(event_t *event);

#endif  /* XROAR_EVENT_H_ */
