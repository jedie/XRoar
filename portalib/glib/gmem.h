/* Portalib - replace common functions from other libraries. */

/* portalib/glib/gmem.h - wrap glib/gmem.h */

#ifndef PORTALIB_GLIB_GMEM_H_
#define PORTALIB_GLIB_GMEM_H_

#include "portalib/glib/gtypes.h"

gpointer g_try_malloc(gsize n_bytes);
gpointer g_try_malloc0(gsize n_bytes);
gpointer g_try_realloc(gpointer mem, gsize n_bytes);
gpointer g_malloc(gsize n_bytes);
gpointer g_malloc0(gsize n_bytes);
gpointer g_realloc(gpointer mem, gsize n_bytes);
void g_free(gpointer mem);

#endif  /* def PORTALIB_GLIB_GMEM_H_ */
