/*
 * pl_glib is a set of public domain work-a-likes for some GLib functions and
 * macros.  It is not even slightly close to a complete reimplementation of
 * GLib.
 *
 * String utility functions.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

gchar *g_strdup(const gchar *str) {
	if (!str) return NULL;
	gsize len = strlen(str);
	return g_memdup(str, len + 1);
}

gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2) {
	if (!s1 || !s2)
		return 0;
	while (*s1 && *s2 && (*s1 == *s2 || g_ascii_tolower(*s1) == g_ascii_tolower(*s2))) {
		s1++;
		s2++;
	}
	return g_ascii_tolower(*s1) - g_ascii_tolower(*s2);
}

gint g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n) {
	if (!s1 || !s2)
		return 0;
	while (n > 0 && *s1 && *s2 &&
	       (*s1 == *s2 || g_ascii_tolower(*s1) == g_ascii_tolower(*s2))) {
		s1++;
		s2++;
		n--;
	}
	return (n == 0) ? 0 : g_ascii_tolower(*s1) - g_ascii_tolower(*s2);
}

gchar g_ascii_tolower(gchar c) {
	if (g_ascii_isupper(c))
		return c | 0x20;
	return c;
}

gchar g_ascii_toupper(gchar c) {
	if (g_ascii_islower(c))
		return c & ~0x20;
	return c;
}
