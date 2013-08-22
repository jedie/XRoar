/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Singly-linked lists.
 */

#ifndef PL_GSLIST_H_
#define PL_GSLIST_H_

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
void g_slist_foreach(GSList *list, GFunc func, gpointer user_data);
GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func);
GSList *g_slist_remove(GSList *list, void *data);
GSList *g_slist_to_head(GSList *list, void *data);
GSList *g_slist_to_tail(GSList *list, void *data);

/* Returns element in list: */
GSList *g_slist_find(GSList *list, gconstpointer data);
GSList *g_slist_find_custom(GSList *list, gconstpointer data, GCompareFunc func);

#define g_slist_next(slist) ((slist) ? (((GSList *)(slist))->next) : NULL)

#endif  /* def PL_GSLIST_H_ */
