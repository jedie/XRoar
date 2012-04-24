/* Portalib - replace common functions from other libraries. */

#include <stdlib.h>
#include <stdio.h>
#include "portalib/string.h"
#include "portalib/glib/gtypes.h"
#include "portalib/glib/gmem.h"
#include "portalib/glib/gstrfuncs.h"

gchar *g_strdup(const gchar *str) {
	if (!str) return NULL;
	gchar *new_str = g_malloc(strlen(str) + 1);
	strcpy(new_str, str);
	return new_str;
}
