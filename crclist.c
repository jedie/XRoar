/*  Copyright 2003-2012 Ciaran Anscomb
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "portalib/glib.h"

#include "types.h"
#include "crclist.h"
#include "xroar.h"

/* User defined CRC lists */
struct crclist {
	GSList *list;
	int flag;
};

/* Hash containing all defined CRC lists */
static GHashTable *crclist_hash = NULL;

/**************************************************************************/

static void init_crclist_hash(void) {
	if (crclist_hash)
		return;

	crclist_hash = g_hash_table_new(g_str_hash, g_str_equal);

	/* Dragon BASIC */
	crclist_assign("d64_1=0x84f68bf9,0x60a4634c,@woolham_d64_1");
	crclist_assign("d64_2=0x17893a42,@woolham_d64_2");
	crclist_assign("d32=0xe3879310,@woolham_d32");
	crclist_assign("dragon=@d64_1,@d32");
	crclist_assign("woolham_d64_1=0xee33ae92");
	crclist_assign("woolham_d64_2=0x1660ae35");
	crclist_assign("woolham_d32=0xff7bf41e,0x9c7eed69");
	/* CoCo BASIC */
	crclist_assign("bas10=0x00b50aaa");
	crclist_assign("bas11=0x6270955a");
	crclist_assign("bas12=0x54368805");
	crclist_assign("bas13=0xd8f4d15e");
	crclist_assign("coco=@bas13,@bas12,@bas11,@bas10");
	crclist_assign("extbas10=0x6111a086");
	crclist_assign("extbas11=0xa82a6254");
	crclist_assign("cocoext=@extbas11,@extbas10");
}

static struct crclist *new_crclist(void) {
	struct crclist *new = g_malloc(sizeof(struct crclist));
	new->list = NULL;
	new->flag = 0;
	return new;
}

static void free_crclist(struct crclist *crclist) {
	if (!crclist) return;
	GSList *list = crclist->list;
	while (list) {
		g_free(list->data);
		list = g_slist_remove(list, list);
	}
	g_free(crclist);
}

/* Parse an assignment string of the form "LIST=ROMNAME[,ROMNAME]...".
 * Overwrites any existing list with name LIST. */
void crclist_assign(const char *astring) {
	init_crclist_hash();
	if (!astring) return;
	char *tmp = g_strdup(astring);
	char *name = strtok(tmp, "=");
	if (!name) return;
	struct crclist *new_list = new_crclist();
	/* find if there's an old list with this name */
	struct crclist *old_list = g_hash_table_lookup(crclist_hash, name);
	if (old_list) {
		/* if so, remove its reference in crclist_hash */
		g_hash_table_remove(crclist_hash, name);
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
		free_crclist(old_list);
	}
	/* add new list to crclist_list */
	g_hash_table_insert(crclist_hash, g_strdup(name), new_list);
	g_free(tmp);
}

/* convert a string to integer and compare against CRC */
static int crc_match(const char *crc_string, uint32_t crc) {
	long long check = strtoll(crc_string, NULL, 16);
	return (uint32_t)(check & 0xffffffff) == crc;
}

/* Match a provided CRC with values in a list.  Returns 1 if found. */
int crclist_match(const char *name, uint32_t crc) {
	init_crclist_hash();
	if (!name) return 0;
	/* not prefixed with an '@'?  then it's not a list! */
	if (name[0] != '@') {
		return crc_match(name, crc);
	}
	struct crclist *crclist = g_hash_table_lookup(crclist_hash, name+1);
	/* found an appropriate list?  flag it and start scanning it */
	if (!crclist)
		return 0;
	GSList *iter;
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

static void print_crclist_entry(char *key, struct crclist *list, void *user_data) {
	GSList *jter;
	(void)user_data;
	if (strlen(key) > 15) {
		printf("\t%s\n\t%16s", key, "");
	} else {
		printf("\t%-15s ", key);
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
void crclist_print(void) {
	init_crclist_hash();
	printf("CRC lists:\n");
	g_hash_table_foreach(crclist_hash, (GHFunc)print_crclist_entry, NULL);
	exit(0);
}
