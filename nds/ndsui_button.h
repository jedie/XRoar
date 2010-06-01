#ifndef XROAR_NDS_NDSUI_BUTTON_H_
#define XROAR_NDS_NDSUI_BUTTON_H_

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_button(int x, int y, int w, const char *label, int is_toggle);

void ndsui_button_press_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_button_release_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_button_set_label(struct ndsui_component *self, const char *label);
void ndsui_button_set_state(struct ndsui_component *self, int pressed);

#endif  /* XROAR_NDS_NDSUI_BUTTON_H_ */
