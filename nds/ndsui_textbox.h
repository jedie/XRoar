#ifndef __NDSUI_TEXTBOX_H__
#define __NDSUI_TEXTBOX_H__

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_textbox(int x, int y, int w, int max_length);

void ndsui_textbox_set_text(struct ndsui_component *self, const char *text);
char *ndsui_textbox_get_text(struct ndsui_component *self);
void ndsui_textbox_type_char(struct ndsui_component *self, int c);

#endif  /* __NDSUI_TEXTBOX_H__ */
