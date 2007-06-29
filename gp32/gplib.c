#include <gpmem.h>

#include "../types.h"

char *gm_strdup(const char *s1) {
	char *s2;
	size_t i = gm_lstrlen(s1) + 1;
	s2 = gm_malloc(i);
	gm_strcpy(s2, s1);
	return s2;
}
