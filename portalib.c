/* Public domain portability stuff */

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "portalib.h"

#ifdef NEED_STRSEP
char *strsep(char **stringp, const char *delim) {
	char *token, *tmp;
	if (stringp == NULL || *stringp == NULL)
		return NULL;
	token = *stringp;
	tmp = token + strcspn(token, delim);
	if (*tmp != '\0')
		*tmp++ = '\0';
	else
		tmp = NULL;
	*stringp = tmp;
	return token;
}
#endif
