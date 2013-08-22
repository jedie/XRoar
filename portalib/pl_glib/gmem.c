/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Memory management.
 *
 * GLib proper has its own slice allocation stuff that makes this more
 * efficient.  These replacements mostly just wrap the equivalent standard C
 * library functions.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pl_glib.h"

gpointer g_try_malloc(gsize n_bytes) {
	return malloc(n_bytes);
}

gpointer g_try_malloc0(gsize n_bytes) {
	gpointer mem = malloc(n_bytes);
	if (mem)
		memset(mem, 0, n_bytes);
	return mem;
}

gpointer g_try_realloc(gpointer mem, gsize n_bytes) {
	if (n_bytes == 0) {
		if (mem)
			g_free(mem);
		return NULL;
	}
	return realloc(mem, n_bytes);
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
	if (mem)
		free(mem);
}

gpointer g_memdup(gconstpointer mem, guint byte_size) {
	if (!mem)
		return NULL;
	void *new_mem = g_malloc(byte_size);
	memcpy(new_mem, mem, byte_size);
	return new_mem;
}
