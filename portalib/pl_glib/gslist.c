/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Singly-linked lists.
 */

#include "config.h"

#include <stdlib.h>

#include "pl_glib.h"

/* Wrap data in a new list container */
static GSList *g_slist_new(void *data) {
	GSList *new;
	new = malloc(sizeof(GSList));
	if (!new) return NULL;
	new->next = NULL;
	new->data = data;
	return new;
}

/* Insert new data before given position */
GSList *g_slist_insert_before(GSList *list, GSList *before, void *data) {
	GSList *elem = g_slist_new(data);
	GSList *iter;
	if (!elem) return list;
	if (!list) return elem;
	elem->next = before;
	if (before == list) return elem;
	for (iter = list; iter; iter = iter->next) {
		if (!iter->next || iter->next == before) {
			iter->next = elem;
			break;
		}
	}
	return list;
}

/* Add new data to head of list */
GSList *g_slist_prepend(GSList *list, void *data) {
	return g_slist_insert_before(list, list, data);
}

/* Add new data to tail of list */
GSList *g_slist_append(GSList *list, void *data) {
	return g_slist_insert_before(list, NULL, data);
}

GSList *g_slist_concat(GSList *list1, GSList *list2) {
	if (!list1) return list2;
	if (!list2) return list1;
	GSList *iter;
	for (iter = list1; iter->next; iter = iter->next);
	iter->next = list2;
	return list1;
}

void g_slist_foreach(GSList *list, GFunc func, gpointer user_data) {
	GSList *iter;
	for (iter = list; iter; iter = iter->next) {
		func(iter->data, user_data);
	}
}

/* Using compare function, insert item at appropriate place in list */
GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func) {
	GSList *elem;
	if (!func)
		return g_slist_prepend(list, data);
	for (elem = list; elem; elem = elem->next) {
		if (func(data, elem->data) <= 0)
			break;
	}
	return g_slist_insert_before(list, elem, data);
}

/* Delete list element containing data */
GSList *g_slist_remove(GSList *list, void *data) {
	GSList **elemp;
	if (!data) return list;
	for (elemp = &list; *elemp; elemp = &(*elemp)->next) {
		if ((*elemp)->data == data) break;
	}
	if (*elemp) {
		GSList *elem = *elemp;
		*elemp = elem->next;
		free(elem);
	}
	return list;
}

/* Move existing list element containing data to head of list */
GSList *g_slist_to_head(GSList *list, void *data) {
	if (!data) return list;
	list = g_slist_remove(list, data);
	return g_slist_prepend(list, data);
}

/* Move existing list element containing data to tail of list */
GSList *g_slist_to_tail(GSList *list, void *data) {
	if (!data) return list;
	list = g_slist_remove(list, data);
	return g_slist_append(list, data);
}

/* Find list element containing data */
GSList *g_slist_find(GSList *list, gconstpointer data) {
	GSList *elem;
	for (elem = list; elem; elem = elem->next) {
		if (elem->data == data)
			return elem;
	}
	return NULL;
}

/* Find list element containing data */
GSList *g_slist_find_custom(GSList *list, gconstpointer data, GCompareFunc func) {
	GSList *elem;
	for (elem = list; elem; elem = elem->next) {
		if (func(elem->data, data) == 0)
			return elem;
	}
	return NULL;
}
