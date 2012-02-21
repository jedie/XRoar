/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2012  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "list.h"
#include "misc.h"
#include "path.h"
#include "romlist.h"
#include "xroar.h"

/* Built-in rom lists */
struct romlist_builtin {
	const char *name;
	const char **list;
	int flag;
};

/* User defined rom lists */
struct romlist {
	char *name;
	struct list *list;
	int flag;
};

/* List of all user defined rom lists */
static struct list *romlist_list = NULL;

/* Fallback Dragon BASIC */
static const char *romlist_dragon[] = { "dragon", NULL };
static const char *romlist_d64_1[] = { "d64_1", "d64rom1", "Dragon Data Ltd - Dragon 64 - IC17", "dragrom", NULL };
static const char *romlist_d64_2[] = { "d64_2", "d64rom2", "Dragon Data Ltd - Dragon 64 - IC18", NULL };
static const char *romlist_d32[] = { "d32", "dragon32", "d32rom", "Dragon Data Ltd - Dragon 32 - IC17", NULL };
/* Specific Dragon BASIC */
static const char *romlist_dragon64[] = { "@d64_1", "@dragon", NULL };
static const char *romlist_dragon64_alt[] = { "@d64_2", NULL };
static const char *romlist_dragon32[] = { "@d32", "@dragon", NULL };
/* Fallback CoCo BASIC */
static const char *romlist_coco[] = { "bas13", "bas12", "bas11", "bas10", NULL };
static const char *romlist_coco_ext[] = { "extbas11", "extbas10", NULL };
/* Specific CoCo BASIC */
static const char *romlist_coco1[] = { "bas10", "@coco", NULL };
static const char *romlist_coco1e[] = { "bas11", "@coco", NULL };
static const char *romlist_coco1e_ext[] = { "extbas10", "@coco_ext", NULL };
static const char *romlist_coco2[] = { "bas12", "@coco", NULL };
static const char *romlist_coco2_ext[] = { "extbas11", "@coco_ext", NULL };
static const char *romlist_coco2b[] = { "bas13", "@coco", NULL };

/* DragonDOS */
static const char *romlist_dragondos[] = { "ddos40", "ddos15", "ddos10", "Dragon Data Ltd - DragonDOS 1.0", NULL };
static const char *romlist_dosplus[] = { "dplus49b", "dplus48", "dosplus-4.8", "DOSPLUS", NULL };
static const char *romlist_superdos[] = { "sdose6", "PNP - SuperDOS E6", "sdose5", "sdose4", NULL };
static const char *romlist_cumana[] = { "cdos20", "CDOS20", NULL };
static const char *romlist_dragondos_compat[] = { "@dosplus", "@superdos", "@dragondos", "@cumana", NULL };
/* RSDOS */
static const char *romlist_rsdos[] = { "disk11", "disk10", NULL };
/* Delta */
static const char *romlist_delta[] = { "delta", "deltados", "Premier Micros - DeltaDOS", NULL };

static struct romlist_builtin romlist_builtins[] = {
	{ .name = "dragon", .list = romlist_dragon, },
	{ .name = "d64_1", .list = romlist_d64_1, },
	{ .name = "d64_2", .list = romlist_d64_2, },
	{ .name = "d32", .list = romlist_d32, },
	{ .name = "dragon64", .list = romlist_dragon64, },
	{ .name = "dragon64_alt", .list = romlist_dragon64_alt, },
	{ .name = "dragon32", .list = romlist_dragon32, },
	{ .name = "coco", .list = romlist_coco, },
	{ .name = "coco_ext", .list = romlist_coco_ext, },
	{ .name = "coco1", .list = romlist_coco1, },
	{ .name = "coco1e", .list = romlist_coco1e, },
	{ .name = "coco1e_ext", .list = romlist_coco1e_ext, },
	{ .name = "coco2", .list = romlist_coco2, },
	{ .name = "coco2_ext", .list = romlist_coco2_ext, },
	{ .name = "coco2b", .list = romlist_coco2b, },
	{ .name = "dragondos", .list = romlist_dragondos, },
	{ .name = "dosplus", .list = romlist_dosplus, },
	{ .name = "superdos", .list = romlist_superdos, },
	{ .name = "cumana", .list = romlist_cumana, },
	{ .name = "dragondos_compat", .list = romlist_dragondos_compat, },
	{ .name = "rsdos", .list = romlist_rsdos, },
	{ .name = "delta", .list = romlist_delta, },
};
#define NUM_ROM_LIST_BUILTINS (int)(sizeof(romlist_builtins)/sizeof(struct romlist_builtin))

static const char *rom_extensions[] = {
	"", ".rom", ".ROM", ".dgn", ".DGN"
};
#define NUM_ROM_EXTENSIONS (int)(sizeof(rom_extensions)/sizeof(const char *))

/**************************************************************************/

static struct romlist_builtin *builtin_by_name(const char *name) {
	int i;
	if (!name) return NULL;
	for (i = 0; i < NUM_ROM_LIST_BUILTINS; i++) {
		if (0 == strcmp(name, romlist_builtins[i].name))
			return &romlist_builtins[i];
	}
	return NULL;
}

static struct romlist *user_defined_by_name(const char *name) {
	struct list *iter;
	/* scan user defined rom lists */
	for (iter = romlist_list; iter; iter = iter->next) {
		struct romlist *tmp = iter->data;
		if (0 == strcmp(tmp->name, name)) {
			return tmp;
		}
	}
	return NULL;
}

static struct list *list_from_builtin(const char *name) {
	int i;
	struct romlist_builtin *builtin = builtin_by_name(name);
	if (!builtin) return NULL;
	if (!builtin->list) return NULL;
	struct list *list = NULL;
	for (i = 0; builtin->list[i]; i++) {
		list = list_append(list, strdup(builtin->list[i]));
	}
	return list;
}

static struct romlist *new_romlist(const char *name) {
	struct romlist *new = xmalloc(sizeof(struct romlist));
	new->name = strdup(name);
	new->list = NULL;
	new->flag = 0;
	return new;
}

static void free_romlist(struct romlist *romlist) {
	if (!romlist) return;
	struct list *list = romlist->list;
	while (list) {
		free(list->data);
		list = list_delete(list, list);
	}
	free(romlist->name);
	free(romlist);
}

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]...".
 * Overwrites any existing list with name LIST. */
void romlist_assign(const char *astring) {
	if (!astring) return;
	char *tmp = strdup(astring);
	char *name = strtok(tmp, "=");
	if (!name) return;
	struct romlist *new_list = new_romlist(name);
	/* find if there's an old list with this name */
	struct romlist *old_list = user_defined_by_name(name);;
	if (old_list) {
		/* if so, remove it's reference in romlist_list */
		romlist_list = list_delete(romlist_list, old_list);
	}
	char *value;
	while ((value = strtok(NULL, "\n\v\f\r,"))) {
		if (value[0] == '@' && 0 == strcmp(value+1, name)) {
			/* reference to this list - append current contents */
			struct list **l;
			for (l = &new_list->list; *l; l = &((*l)->next));
			if (old_list) {
				*l = old_list->list;
				old_list->list = NULL;
			} else {
				/* attempt to create it from builtin */
				*l = list_from_builtin(name);
			}
		} else {
			/* otherwise just add a new entry */
			new_list->list = list_append(new_list->list, strdup(value));
		}
	}
	if (old_list) {
		free_romlist(old_list);
	}
	free(tmp);
	/* add new list to romlist_list */
	romlist_list = list_append(romlist_list, new_list);
}

/* Find a ROM within ROMPATH */
static char *find_rom(const char *romname) {
	char *filename;
	char *path = NULL;
	int i;
	if (!romname) return NULL;
	filename = xmalloc(strlen(romname) + 5);
	for (i = 0; i < NUM_ROM_EXTENSIONS; i++) {
		strcpy(filename, romname);
		strcat(filename, rom_extensions[i]);
		path = find_in_path(xroar_rom_path, filename);
		if (path) break;
	}
	free(filename);
	return path;
}

/* Attempt to find a ROM image.  If name starts with '@', search the named
 * list for the first accessible entry, otherwise search for a single entry. */
char *romlist_find(const char *name) {
	if (!name) return NULL;
	char *path = NULL;
	/* not prefixes with an '@'?  then it's not a list! */
	if (name[0] != '@') {
		return find_rom(name);
	}
	struct romlist *romlist = user_defined_by_name(name+1);
	/* found an appropriate list?  flag it and start scanning it */
	if (romlist) {
		struct list *iter;
		if (romlist->flag)
			return NULL;
		romlist->flag = 1;
		for (iter = romlist->list; iter; iter = iter->next) {
			char *ent = iter->data;
			if (ent) {
				if (ent[0] == '@') {
					path = romlist_find(ent);
					break;
				}
				if ((path = find_rom(ent))) {
					break;
				}
			}
		}
		romlist->flag = 0;
		return path;
	}
	/* no?  fall back to the builtins */
	struct romlist_builtin *builtin = builtin_by_name(name+1);
	if (!builtin) return NULL;
	if (!builtin->list) return NULL;
	if (builtin->flag) return NULL;
	builtin->flag = 1;
	int i;
	for (i = 0; builtin->list[i]; i++) {
		const char *ent = builtin->list[i];
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
	builtin->flag = 0;
	return path;
}

/* Print a list of defined ROM lists to stdout */
void romlist_print(void) {
	int i, j;
	struct list *iter;
	printf("Built-in ROM lists:\n");
	for (i = 0; i < NUM_ROM_LIST_BUILTINS; i++) {
		if (strlen(romlist_builtins[i].name) > 15) {
			printf("\t%s\n\t%16s", romlist_builtins[i].name, "");
		} else {
			printf("\t%-15s ", romlist_builtins[i].name);
		}
		for (j = 0; romlist_builtins[i].list[j]; j++) {
			printf("%s", romlist_builtins[i].list[j]);
			if (romlist_builtins[i].list[j+1]) {
				putchar(',');
			}
		}
		printf("\n");
	}
	if (romlist_list)
		printf("User-defined ROM lists:\n");
	for (iter = romlist_list; iter; iter = iter->next) {
		struct romlist *list = iter->data;
		struct list *jter;
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
	exit(0);
}
