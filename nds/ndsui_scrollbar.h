#ifndef __NDSUI_SCROLLBAR_H__
#define __NDSUI_SCROLLBAR_H__

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_scrollbar(int x, int y, int w, int h);
void ndsui_scrollbar_set_total(struct ndsui_component *self, int total);
void ndsui_scrollbar_set_visible(struct ndsui_component *self, int visible);
void ndsui_scrollbar_set_offset(struct ndsui_component *self, int offset);

void ndsui_scrollbar_offset_callback(struct ndsui_component *self, void (*func)(int offset));

#endif  /* __NDSUI_SCROLLBAR_H__ */
