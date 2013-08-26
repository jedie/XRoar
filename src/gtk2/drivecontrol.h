/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_GTK2_DRIVECONTROL_H_
#define XROAR_GTK2_DRIVECONTROL_H_

struct vdisk;

void gtk2_insert_disk(int drive);
void gtk2_create_dc_window(void);
void gtk2_toggle_dc_window(GtkToggleAction *current, gpointer user_data);

void gtk2_update_drive_disk(int drive, struct vdisk *disk);
void gtk2_update_drive_write_enable(int drive, _Bool write_enable);
void gtk2_update_drive_write_back(int drive, _Bool write_back);

#endif  /* XROAR_GTK2_DRIVECONTROL_H_ */
