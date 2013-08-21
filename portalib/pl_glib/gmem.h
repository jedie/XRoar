/* Portalib - replace common functions from other libraries. */

#ifndef PORTALIB_GLIB_GMEM_H_
#define PORTALIB_GLIB_GMEM_H_

#include "pl_glib/gtypes.h"

gpointer g_try_malloc(gsize n_bytes);
gpointer g_try_malloc0(gsize n_bytes);
gpointer g_try_realloc(gpointer mem, gsize n_bytes);
gpointer g_malloc(gsize n_bytes);
gpointer g_malloc0(gsize n_bytes);
gpointer g_realloc(gpointer mem, gsize n_bytes);
void g_free(gpointer mem);
gpointer g_memdup(gconstpointer mem, guint byte_size);

#endif  /* def PORTALIB_GLIB_GMEM_H_ */
