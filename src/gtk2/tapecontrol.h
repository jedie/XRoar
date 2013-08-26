/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_GTK2_TAPECONTROL_H_
#define XROAR_GTK2_TAPECONTROL_H_

void gtk2_create_tc_window(void);
void gtk2_toggle_tc_window(GtkToggleAction *current, gpointer user_data);

void gtk2_input_tape_filename_cb(const char *filename);
void gtk2_output_tape_filename_cb(const char *filename);
void gtk2_update_tape_state(int flags);

#endif  /* XROAR_GTK2_TAPECONTROL_H_ */
