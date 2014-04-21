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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "pl-alloca.h"
#include "slist.h"
#include "xalloc.h"

#include "path.h"
#include "romlist.h"
#include "xroar.h"

/* User defined rom lists */
struct romlist {
	char *name;
	struct slist *list;
	_Bool flag;
};

/* List containing all defined rom lists */
static struct slist *romlist_list = NULL;

static char const * const rom_extensions[] = {
	"", ".rom", ".ROM", ".dgn", ".DGN"
};

static int compare_entry(struct romlist *a, char *b) {
	return strcmp(a->name, b);
}

/**************************************************************************/

static struct romlist *new_romlist(const char *name) {
	struct romlist *new = xmalloc(sizeof(*new));
	new->name = xstrdup(name);
	new->list = NULL;
	new->flag = 0;
	return new;
}

static void free_romlist(struct romlist *romlist) {
	if (!romlist) return;
	struct slist *list = romlist->list;
	while (list) {
		void *data = list->data;
		list = slist_remove(list, data);
		free(data);
	}
	free(romlist->name);
	free(romlist);
}

static struct romlist *find_romlist(const char *name) {
	struct slist *entry = slist_find_custom(romlist_list, name, (slist_cmp_func)compare_entry);
	if (!entry) return NULL;
	return entry->data;
}

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]...".
 * Overwrites any existing list with name LIST. */
void romlist_assign(const char *astring) {
	if (!astring) return;
	char *tmp = alloca(strlen(astring) + 1);
	strcpy(tmp, astring);
	char *name = strtok(tmp, "=");
	if (!name) return;
	struct romlist *new_list = new_romlist(name);
	/* find if there's an old list with this name */
	struct romlist *old_list = find_romlist(name);
	if (old_list) {
		/* if so, remove its reference in romlist_list */
		romlist_list = slist_remove(romlist_list, old_list);
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
		free_romlist(old_list);
	}
	/* add new list to romlist_list */
	romlist_list = slist_append(romlist_list, new_list);
}

/* Find a ROM within ROMPATH */
static char *find_rom(const char *romname) {
	char *filename;
	char *path = NULL;
	if (!romname) return NULL;
	filename = alloca(strlen(romname) + 5);
	for (unsigned i = 0; i < ARRAY_N_ELEMENTS(rom_extensions); i++) {
		strcpy(filename, romname);
		strcat(filename, rom_extensions[i]);
		path = find_in_path(xroar_rom_path, filename);
		if (path) break;
	}
	return path;
}

/* Attempt to find a ROM image.  If name starts with '@', search the named
 * list for the first accessible entry, otherwise search for a single entry. */
char *romlist_find(const char *name) {
	if (!name) return NULL;
	char *path = NULL;
	/* not prefixed with an '@'?  then it's not a list! */
	if (name[0] != '@') {
		return find_rom(name);
	}
	struct romlist *romlist = find_romlist(name+1);
	/* found an appropriate list?  flag it and start scanning it */
	if (!romlist)
		return NULL;
	struct slist *iter;
	if (romlist->flag)
		return NULL;
	romlist->flag = 1;
	for (iter = romlist->list; iter; iter = iter->next) {
		char *ent = iter->data;
		if (ent) {
			if (ent[0] == '@') {
				path = romlist_find(ent);
				if (path)
					break;
			} else if ((path = find_rom(ent))) {
				break;
			}
		}
	}
	romlist->flag = 0;
	return path;
}

static void print_romlist_entry(struct romlist *list, void *user_data) {
	struct slist *jter;
	if (user_data) {
		printf("romlist %s=", list->name);
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
void romlist_print_all(void) {
	slist_foreach(romlist_list, (slist_iter_func)print_romlist_entry, (void *)1);
}

/* Print list and exit */
void romlist_print(void) {
	printf("ROM lists:\n");
	slist_foreach(romlist_list, (slist_iter_func)print_romlist_entry, NULL);
	exit(EXIT_SUCCESS);
}
