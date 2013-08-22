/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Hash tables.
 */

#ifndef PL_GLIB_GHASH_H_
#define PL_GLIB_GHASH_H_

#include <stdlib.h>
#include "pl_glib/gtypes.h"

struct _GHashTable;
typedef struct _GHashTable GHashTable;

GHashTable *g_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func);
void g_hash_table_insert(GHashTable *hash_table, gpointer key, gpointer value);
gboolean g_hash_table_remove(GHashTable *hash_table, gconstpointer key);
gpointer g_hash_table_lookup(GHashTable *hash_table, gconstpointer key);
void g_hash_table_foreach(GHashTable *hash_table, GHFunc func, gpointer user_data);

/* Hash functions - the actual hash functions are dummies */
gboolean g_str_equal(gconstpointer v1, gconstpointer v2);
#define g_str_hash NULL

#endif  /* def PL_GLIB_GHASH_H_ */
