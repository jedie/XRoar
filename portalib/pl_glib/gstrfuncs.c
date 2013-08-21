/* Portalib - replace common functions from other libraries. */

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
