/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __EVENT_H__
#define __EVENT_H__

typedef struct event_t event_t;
struct event_t {
	Cycle at_cycle;
	void (*dispatch)(void *context);
	void *context;
	int queued;
	event_t *next;
};

extern event_t *event_list;

#define EVENT_EXISTS (event_list)
#define EVENT_PENDING (event_list && \
		(int)(current_cycle - event_list->at_cycle) >= 0)
#define DISPATCH_NEXT_EVENT do { \
		event_t *e = event_list; \
		event_list = event_list->next; \
		e->queued = 0; \
		e->dispatch(e->context); \
	} while (0)

event_t *event_new(void);
void event_free(event_t *event);
void event_queue(event_t *event);
void event_dequeue(event_t *event);

#endif  /* __EVENT_H__ */
