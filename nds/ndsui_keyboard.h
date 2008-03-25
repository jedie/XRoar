#ifndef __NDSUI_KEYBOARD_H__
#define __NDSUI_KEYBOARD_H__

#include "nds/ndsui.h"

struct ndsui_component *new_ndsui_keyboard(int x, int y, int shift_is_toggle);

void ndsui_keyboard_keypress_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_keyrelease_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_shift_callback(struct ndsui_component *self, void (*func)(int));
void ndsui_keyboard_set_shift_state(struct ndsui_component *self, int state);

#endif  /* __NDSUI_KEYBOARD_H__ */
