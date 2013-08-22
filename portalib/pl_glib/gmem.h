/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * Memory management.
 */

#ifndef PL_GLIB_GMEM_H_
#define PL_GLIB_GMEM_H_

#include "pl_glib/gtypes.h"

gpointer g_try_malloc(gsize n_bytes);
gpointer g_try_malloc0(gsize n_bytes);
gpointer g_try_realloc(gpointer mem, gsize n_bytes);
gpointer g_malloc(gsize n_bytes);
gpointer g_malloc0(gsize n_bytes);
gpointer g_realloc(gpointer mem, gsize n_bytes);
void g_free(gpointer mem);
gpointer g_memdup(gconstpointer mem, guint byte_size);

#endif  /* def PL_GLIB_GMEM_H_ */
