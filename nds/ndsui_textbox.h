#ifndef XROAR_NDS_NDSUI_TEXTBOX_H_
#define XROAR_NDS_NDSUI_TEXTBOX_H_

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_textbox(int x, int y, int w, int max_length);

void ndsui_textbox_set_text(struct ndsui_component *self, const char *text);
char *ndsui_textbox_get_text(struct ndsui_component *self);
void ndsui_textbox_type_char(struct ndsui_component *self, int c);

#endif  /* XROAR_NDS_NDSUI_TEXTBOX_H_ */
