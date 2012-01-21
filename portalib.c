/* Functions to "fill in" possible libc omissions. */

/* Functions taken from from musl libc, © 2005-2011 Rich Felker. */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "portalib.h"

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

#ifdef NEED_STRCASECMP
int strcasecmp(const char *_l, const char *_r) {
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}
#endif
