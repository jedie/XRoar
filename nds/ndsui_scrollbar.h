#ifndef XROAR_NDS_NDSUI_SCROLLBAR_H_
#define XROAR_NDS_NDSUI_SCROLLBAR_H_

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_scrollbar(int x, int y, int w, int h);
void ndsui_scrollbar_set_total(struct ndsui_component *self, int total);
void ndsui_scrollbar_set_visible(struct ndsui_component *self, int visible);
void ndsui_scrollbar_set_offset(struct ndsui_component *self, int offset);

void ndsui_scrollbar_offset_callback(struct ndsui_component *self, void (*func)(int offset));

#endif  /* XROAR_NDS_NDSUI_SCROLLBAR_H_ */
