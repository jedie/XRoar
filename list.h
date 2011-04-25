/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_LIST_H_
#define XROAR_LIST_H_

struct list {
	struct list *next;
	void *data;
};

/* Each of these return the new pointer to the head of the list: */
struct list *list_insert_before(struct list *list, struct list *before, void *data);
struct list *list_prepend(struct list *list, void *data);
struct list *list_append(struct list *list, void *data);
struct list *list_delete(struct list *list, void *data);
struct list *list_to_head(struct list *list, void *data);
struct list *list_to_tail(struct list *list, void *data);

/* Returns element in list: */
struct list *list_find(struct list *list, void *data);

#endif  /* def XROAR_LIST_H_ */
