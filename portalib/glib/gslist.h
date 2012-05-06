/* Portalib - replace common functions from other libraries. */

/* portalib/glib/gslist.h - wrap glib/gslist.h */

#ifndef PORTALIB_GSLIST_H_
#define PORTALIB_GSLIST_H_

struct GSList;
typedef struct GSList GSList;

struct GSList {
	GSList *next;
	void *data;
};

/* Each of these return the new pointer to the head of the list: */
GSList *g_slist_insert_before(GSList *list, GSList *before, void *data);
GSList *g_slist_prepend(GSList *list, void *data);
GSList *g_slist_append(GSList *list, void *data);
GSList *g_slist_concat(GSList *list1, GSList *list2);
GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func);
GSList *g_slist_remove(GSList *list, void *data);
GSList *g_slist_to_head(GSList *list, void *data);
GSList *g_slist_to_tail(GSList *list, void *data);

/* Returns element in list: */
GSList *g_slist_find(GSList *list, void *data);

#define g_slist_next(slist) ((slist) ? (((GSList *)(slist))->next) : NULL)

#endif  /* def PORTALIB_GSLIST_H_ */
