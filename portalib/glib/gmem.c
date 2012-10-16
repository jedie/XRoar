/* Portalib - replace common functions from other libraries. */

/* GLib proper has its own slice allocation stuff that makes this more
 * efficient.  These replacements just wrap the standard syscalls. */

#include <stdlib.h>
#include <stdio.h>
#include "portalib/string.h"
#include "portalib/glib/gtypes.h"
#include "portalib/glib/gmem.h"

gpointer g_try_malloc(gsize n_bytes) {
	return malloc(n_bytes);
}

gpointer g_try_malloc0(gsize n_bytes) {
	gpointer mem = malloc(n_bytes);
	if (!mem) return NULL;
	memset(mem, 0, n_bytes);
	return mem;
}

gpointer g_try_realloc(gpointer mem, gsize n_bytes) {
	void *new_mem = realloc(mem, n_bytes);
	if (new_mem && n_bytes == 0) {
		g_free(new_mem);
		return NULL;
	}
	return new_mem;
}

gpointer g_malloc(gsize n_bytes) {
	void *mem = g_try_malloc(n_bytes);
	if (!mem) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	return mem;
}

gpointer g_malloc0(gsize n_bytes) {
	void *mem = g_malloc(n_bytes);
	memset(mem, 0, n_bytes);
	return mem;
}

gpointer g_realloc(gpointer mem, gsize n_bytes) {
	void *new_mem = g_try_realloc(mem, n_bytes);
	if (!new_mem && n_bytes != 0) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	return new_mem;
}

void g_free(gpointer mem) {
	free(mem);
}
