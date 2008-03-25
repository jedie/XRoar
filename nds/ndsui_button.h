#ifndef __NDSUI_BUTTON_H__
#define __NDSUI_BUTTON_H__

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_button(int x, int y, const char *label, int id, int is_toggle);

void ndsui_button_press_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_button_release_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_button_set_label(struct ndsui_component *self, const char *label);
void ndsui_button_set_state(struct ndsui_component *self, int pressed);

#endif  /* __NDSUI_BUTTON_H__ */
