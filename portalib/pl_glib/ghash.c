/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Hash tables.
 *
 * This implementation is very stupid: not really a hash at all, just a linked
 * list full of key/value pairs.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

struct list_entry {
	gpointer key;
	gpointer value;
};

struct _GHashTable {
	GEqualFunc key_equal_func;
	GSList *list;
};

GHashTable *g_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func) {
	GHashTable *this = malloc(sizeof(GHashTable));
	if (!this) return NULL;
	(void)hash_func;  /* stupid implementation */
	this->key_equal_func = key_equal_func;
	this->list = NULL;
	return this;
}

void g_hash_table_insert(GHashTable *hash_table, gpointer key, gpointer value) {
	struct list_entry *entry;
	if (!hash_table) return;
	entry = malloc(sizeof(struct list_entry));
	if (!entry) return;
	entry->key = key;
	entry->value = value;
	hash_table->list = g_slist_append(hash_table->list, entry);
}

static struct list_entry *find_entry_by_key(GHashTable *hash_table, gconstpointer key) {
	if (!hash_table) return NULL;
	if (!hash_table->list) return NULL;
	GEqualFunc equals = hash_table->key_equal_func;
	GSList *iter;
	for (iter = hash_table->list; iter; iter = g_slist_next(iter)) {
		struct list_entry *entry = (struct list_entry *)iter->data;
		if (!equals && key == entry->key)
			return entry;
		if (equals && equals(key, entry->key))
			return entry;
	}
	return NULL;
}

gboolean g_hash_table_remove(GHashTable *hash_table, gconstpointer key) {
	struct list_entry *entry = find_entry_by_key(hash_table, key);
	if (!entry) return FALSE;
	hash_table->list = g_slist_remove(hash_table->list, entry);
	free(entry);
	return TRUE;
}

gpointer g_hash_table_lookup(GHashTable *hash_table, gconstpointer key) {
	struct list_entry *entry = find_entry_by_key(hash_table, key);
	if (!entry) return NULL;
	return entry->value;
}

void g_hash_table_foreach(GHashTable *hash_table, GHFunc func, gpointer user_data) {
	if (!hash_table) return;
	if (!func) return;
	GSList *iter;
	for (iter = hash_table->list; iter; iter = g_slist_next(iter)) {
		struct list_entry *entry = (struct list_entry *)iter->data;
		func(entry->key, entry->value, user_data);
	}
}

gboolean g_str_equal(gconstpointer v1, gconstpointer v2) {
	return (0 == strcmp((const char *)v1, (const char *)v2));
}
