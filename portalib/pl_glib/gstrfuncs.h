/* Portalib - replace common functions from other libraries. */

#ifndef PORTALIB_GLIB_GSTRFUNCS_H_
#define PORTALIB_GLIB_GSTRFUNCS_H_

#include "pl_glib/gtypes.h"

gchar *g_strdup(const gchar *str);
gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2);
gint g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n);
gchar g_ascii_tolower(gchar c);
gchar g_ascii_toupper(gchar c);

static inline gboolean g_ascii_islower(gchar c) {
	return (c >= 'a' && c <= 'z');
}

static inline gboolean g_ascii_isupper(gchar c) {
	return (c >= 'A' && c <= 'Z');
}

#endif  /* def PORTALIB_GLIB_GSTRFUNCS_H_ */
