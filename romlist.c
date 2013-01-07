/*  Copyright 2003-2013 Ciaran Anscomb
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
#include "portalib/glib.h"

#include "path.h"
#include "romlist.h"
#include "xroar.h"

/* User defined rom lists */
struct romlist {
	char *name;
	GSList *list;
	_Bool flag;
};

/* List containing all defined rom lists */
static _Bool romlist_initialised = 0;
static GSList *romlist_list = NULL;

static const char *rom_extensions[] = {
	"", ".rom", ".ROM", ".dgn", ".DGN"
};
#define NUM_ROM_EXTENSIONS (int)(sizeof(rom_extensions)/sizeof(const char *))

static int compare_entry(struct romlist *a, char *b) {
	return strcmp(a->name, b);
}

/**************************************************************************/

static void init_romlist_list(void) {
	if (romlist_initialised)
		return;
	romlist_initialised = 1;

	/* Fallback Dragon BASIC */
	romlist_assign("dragon=dragon");
	romlist_assign("d64_1=d64_1,d64rom1,Dragon Data Ltd - Dragon 64 - IC17,dragrom");
	romlist_assign("d64_2=d64_2,d64rom2,Dragon Data Ltd - Dragon 64 - IC18");
	romlist_assign("d32=d32,dragon32,d32rom,Dragon Data Ltd - Dragon 32 - IC17");
	/* Specific Dragon BASIC */
	romlist_assign("dragon64=@d64_1,@dragon");
	romlist_assign("dragon64_alt=@d64_2");
	romlist_assign("dragon32=@d32,@dragon");
	/* Fallback CoCo BASIC */
	romlist_assign("coco=bas13,bas12,bas11,bas10");
	romlist_assign("coco_ext=extbas11,extbas10");
	/* Specific CoCo BASIC */
	romlist_assign("coco1=bas10,@coco");
	romlist_assign("coco1e=bas11,@coco");
	romlist_assign("coco1e_ext=extbas10,@coco_ext");
	romlist_assign("coco2=bas12,@coco");
	romlist_assign("coco2_ext=extbas11,@coco_ext");
	romlist_assign("coco2b=bas13,@coco");

	/* DragonDOS */
	romlist_assign("dragondos=ddos40,ddos15,ddos10,Dragon Data Ltd - DragonDOS 1.0");
	romlist_assign("dosplus=dplus49b,dplus48,dosplus-4.8,DOSPLUS");
	romlist_assign("superdos=sdose6,PNP - SuperDOS E6,sdose5,sdose4");
	romlist_assign("cumana=cdos20,CDOS20");
	romlist_assign("dragondos_compat=@dosplus,@superdos,@dragondos,@cumana");
	/* RSDOS */
	romlist_assign("rsdos=disk11,disk10");
	/* Delta */
	romlist_assign("delta=delta,deltados,Premier Micros - DeltaDOS");
	/* RSDOS with becker port */
	romlist_assign("rsdos_becker=hdbdw3bck");
}

static struct romlist *new_romlist(const char *name) {
	struct romlist *new = g_malloc(sizeof(struct romlist));
	new->name = g_strdup(name);
	new->list = NULL;
	new->flag = 0;
	return new;
}

static void free_romlist(struct romlist *romlist) {
	if (!romlist) return;
	GSList *list = romlist->list;
	while (list) {
		gpointer data = list->data;
		list = g_slist_remove(list, data);
		g_free(data);
	}
	g_free(romlist->name);
	g_free(romlist);
}

static struct romlist *find_romlist(const char *name) {
	GSList *entry = g_slist_find_custom(romlist_list, name, (GCompareFunc)compare_entry);
	if (!entry) return NULL;
	return entry->data;
}

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]...".
 * Overwrites any existing list with name LIST. */
void romlist_assign(const char *astring) {
	init_romlist_list();
	if (!astring) return;
	char *tmp = g_alloca(strlen(astring) + 1);
	strcpy(tmp, astring);
	char *name = strtok(tmp, "=");
	if (!name) return;
	struct romlist *new_list = new_romlist(name);
	/* find if there's an old list with this name */
	struct romlist *old_list = find_romlist(name);
	if (old_list) {
		/* if so, remove its reference in romlist_list */
		romlist_list = g_slist_remove(romlist_list, old_list);
	}
	char *value;
	while ((value = strtok(NULL, "\n\v\f\r,"))) {
		if (value[0] == '@' && 0 == strcmp(value+1, name)) {
			/* reference to this list - append current contents */
			if (old_list) {
				new_list->list = g_slist_concat(new_list->list, old_list->list);
				old_list->list = NULL;
			}
		} else {
			/* otherwise just add a new entry */
			new_list->list = g_slist_append(new_list->list, g_strdup(value));
		}
	}
	if (old_list) {
		free_romlist(old_list);
	}
	/* add new list to romlist_list */
	romlist_list = g_slist_append(romlist_list, new_list);
}

/* Find a ROM within ROMPATH */
static char *find_rom(const char *romname) {
	char *filename;
	char *path = NULL;
	int i;
	if (!romname) return NULL;
	filename = g_alloca(strlen(romname) + 5);
	for (i = 0; i < NUM_ROM_EXTENSIONS; i++) {
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
	init_romlist_list();
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
	GSList *iter;
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
	GSList *jter;
	(void)user_data;
	if (strlen(list->name) > 15) {
		printf("\t%s\n\t%16s", list->name, "");
	} else {
		printf("\t%-15s ", list->name);
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
void romlist_print(void) {
	init_romlist_list();
	printf("ROM lists:\n");
	g_slist_foreach(romlist_list, (GFunc)print_romlist_entry, NULL);
	exit(EXIT_SUCCESS);
}
