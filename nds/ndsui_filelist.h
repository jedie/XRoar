#ifndef XROAR_NDS_NDSUI_FILELIST_H_
#define XROAR_NDS_NDSUI_FILELIST_H_

#include <sys/dir.h>
#include "nds/ndsui.h"

struct ndsui_filelist_fileinfo {
	char *filename;
	mode_t mode;
};

struct ndsui_component *new_ndsui_filelist(int x, int y, int w, int h);
/* Read directory */
void ndsui_filelist_open(struct ndsui_component *self, const char *path, const char **extensions);
void ndsui_filelist_close(struct ndsui_component *self);
/* Currently selected filename */
char *ndsui_filelist_filename(struct ndsui_component *self);

void ndsui_filelist_set_offset(struct ndsui_component *self, int offset);
void ndsui_filelist_num_files_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_filelist_visible_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_filelist_file_select_callback(struct ndsui_component *self, void (*func)(const char *));
void ndsui_filelist_search_string(struct ndsui_component *self, char *str);

#endif  /* XROAR_NDS_NDSUI_FILELIST_H_ */
