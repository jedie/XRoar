#ifndef __NDSUI_BUTTON_H__
#define __NDSUI_BUTTON_H__

#include <sys/dir.h>
#include "nds/ndsui.h"

struct ndsui_buttno_fileinfo {
	char *filename;
	mode_t mode;
};

struct ndsui_component *new_ndsui_button(int x, int y, const char *label);

void ndsui_button_press_callback(struct ndsui_component *self, void (*func)(void));
void ndsui_button_release_callback(struct ndsui_component *self, void (*func)(void));

#endif  /* __NDSUI_BUTTON_H__ */
