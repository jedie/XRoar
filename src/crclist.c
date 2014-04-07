/*  Copyright 2003-2014 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <alloca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slist.h"
#include "xalloc.h"

#include "crclist.h"
#include "xroar.h"

/* User defined CRC lists */
struct crclist {
	char *name;
	struct slist *list;
	_Bool flag;
};

/* List containing all defined CRC lists */
static struct slist *crclist_list = NULL;

static int compare_entry(struct crclist *a, char *b) {
	return strcmp(a->name, b);
}

/**************************************************************************/

static struct crclist *new_crclist(const char *name) {
	struct crclist *new = xmalloc(sizeof(*new));
	new->name = xstrdup(name);
	new->list = NULL;
	new->flag = 0;
	return new;
}

static void free_crclist(struct crclist *crclist) {
	if (!crclist) return;
	struct slist *list = crclist->list;
	while (list) {
		void *data = list->data;
		list = slist_remove(list, data);
		free(data);
	}
	free(crclist->name);
	free(crclist);
}

static struct crclist *find_crclist(const char *name) {
	struct slist *entry = slist_find_custom(crclist_list, name, (slist_cmp_func)compare_entry);
	if (!entry) return NULL;
	return entry->data;
}

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]...".
 * Overwrites any existing list with name LIST. */
void crclist_assign(const char *astring) {
	if (!astring) return;
	char *tmp = alloca(strlen(astring) + 1);
	strcpy(tmp, astring);
	char *name = strtok(tmp, "=");
	if (!name) return;
	struct crclist *new_list = new_crclist(name);
	/* find if there's an old list with this name */
	struct crclist *old_list = find_crclist(name);
	if (old_list) {
		/* if so, remove its reference in crclist_list */
		crclist_list = slist_remove(crclist_list, old_list);
	}
	char *value;
	while ((value = strtok(NULL, "\n\v\f\r,"))) {
		if (value[0] == '@' && 0 == strcmp(value+1, name)) {
			/* reference to this list - append current contents */
			if (old_list) {
				new_list->list = slist_concat(new_list->list, old_list->list);
				old_list->list = NULL;
			}
		} else {
			/* otherwise just add a new entry */
			new_list->list = slist_append(new_list->list, xstrdup(value));
		}
	}
	if (old_list) {
		free_crclist(old_list);
	}
	/* add new list to crclist_list */
	crclist_list = slist_append(crclist_list, new_list);
}

/* convert a string to integer and compare against CRC */
static int crc_match(const char *crc_string, uint32_t crc) {
	long long check = strtoll(crc_string, NULL, 16);
	return (uint32_t)(check & 0xffffffff) == crc;
}

/* Match a provided CRC with values in a list.  Returns 1 if found. */
int crclist_match(const char *name, uint32_t crc) {
	if (!name) return 0;
	/* not prefixed with an '@'?  then it's not a list! */
	if (name[0] != '@') {
		return crc_match(name, crc);
	}
	struct crclist *crclist = find_crclist(name+1);
	/* found an appropriate list?  flag it and start scanning it */
	if (!crclist)
		return 0;
	struct slist *iter;
	if (crclist->flag)
		return 0;
	crclist->flag = 1;
	int match = 0;
	for (iter = crclist->list; iter; iter = iter->next) {
		char *ent = iter->data;
		if (ent) {
			if (ent[0] == '@') {
				if ((match = crclist_match(ent, crc)))
					break;
			} else if ((match = crc_match(ent, crc))) {
				break;
			}
		}
	}
	crclist->flag = 0;
	return match;
}

static void print_crclist_entry(struct crclist *list, void *user_data) {
	struct slist *jter;
	if (user_data) {
		printf("crclist %s=", list->name);
	} else {
		if (strlen(list->name) > 15) {
			printf("\t%s\n\t%16s", list->name, "");
		} else {
			printf("\t%-15s ", list->name);
		}
	}
	for (jter = list->list; jter; jter = jter->next) {
		printf("%s", (char *)jter->data);
		if (jter->next) {
			putchar(',');
		}
	}
	printf("\n");
}

/* Print a list of defined ROM lists to stdout */
void crclist_print_all(void) {
	slist_foreach(crclist_list, (slist_iter_func)print_crclist_entry, (void *)1);
}

/* Print list and exit */
void crclist_print(void) {
	printf("CRC lists:\n");
	slist_foreach(crclist_list, (slist_iter_func)print_crclist_entry, NULL);
	exit(EXIT_SUCCESS);
}
