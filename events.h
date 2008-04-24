/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2008  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __EVENT_H__
#define __EVENT_H__

typedef struct event_t event_t;
struct event_t {
	Cycle at_cycle;
	void (*dispatch)(void);
	int queued;
	event_t **list;
	event_t *next;
};

extern event_t *event_list;

#define EVENT_EXISTS(l) (l)
#define EVENT_PENDING(l) (l && \
		(int)(current_cycle - l->at_cycle) >= 0)
#define DISPATCH_NEXT_EVENT(l) do { \
		event_t *e = l; \
		l = l->next; \
		e->queued = 0; \
		e->dispatch(); \
	} while (0)

event_t *event_new(void);
void event_init(event_t *event);  /* for static declarations */

void event_free(event_t *event);
void event_queue(event_t **list, event_t *event);
void event_dequeue(event_t *event);

#endif  /* __EVENT_H__ */
