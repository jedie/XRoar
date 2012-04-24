/* Function taken from from musl libc, © 2005-2011 Rich Felker. */

#include "config.h"

#include <stdlib.h>

#include "portalib/string.h"

#ifdef NEED_STRSEP
char *strsep(char **str, const char *sep) {
	char *s = *str, *end;
	if (!s) return NULL;
	end = s + strcspn(s, sep);
	if (*end) *end++ = 0;
	else end = 0;
	*str = end;
	return s;
}
#endif
