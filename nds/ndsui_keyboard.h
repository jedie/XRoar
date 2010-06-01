#ifndef XROAR_NDS_NDSUI_KEYBOARD_H_
#define XROAR_NDS_NDSUI_KEYBOARD_H_

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_keyboard(int x, int y, int shift_is_toggle);

void ndsui_keyboard_keypress_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_keyrelease_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_shift_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_set_shift_state(struct ndsui_component *self, int state);

#endif  /* XROAR_NDS_NDSUI_KEYBOARD_H_ */
