/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_GTK2_UI_GTK2_H_
#define XROAR_GTK2_UI_GTK2_H_

#include "joystick.h"

extern unsigned gtk2_window_x, gtk2_window_y;
extern unsigned gtk2_window_w, gtk2_window_h;

extern struct joystick_interface gtk2_js_if_keyboard;

extern GtkWidget *gtk2_top_window;
extern GtkWidget *gtk2_drawing_area;
extern GtkUIManager *gtk2_menu_manager;

#endif  /* XROAR_GTK2_UI_GTK2_H_ */
