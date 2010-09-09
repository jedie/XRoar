/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_XROAR_H_
#define XROAR_XROAR_H_

#include "types.h"
#include "events.h"

/* Convenient values for arguments to helper functions */
#define XROAR_OFF    (0)
#define XROAR_ON     (1)
#define XROAR_CLEAR  (0)
#define XROAR_SET    (1)
#define XROAR_FALSE  (0)
#define XROAR_TRUE   (1)
#define XROAR_AUTO   (-1)
#define XROAR_TOGGLE (-2)
#define XROAR_CYCLE  (-3)

#define FILETYPE_UNKNOWN (0)
#define FILETYPE_VDK (1)
#define FILETYPE_JVC (2)
#define FILETYPE_DMK (3)
#define FILETYPE_BIN (4)
#define FILETYPE_HEX (5)
#define FILETYPE_CAS (6)
#define FILETYPE_WAV (7)
#define FILETYPE_SNA (8)
#define FILETYPE_ROM (9)
#define FILETYPE_ASC (10)

extern const char *xroar_disk_exts[];
extern const char *xroar_tape_exts[];
extern const char *xroar_snap_exts[];
extern const char *xroar_cart_exts[];

extern void (*xroar_machine_changed_cb)(int machine_type);
extern void (*xroar_dos_changed_cb)(int dos_type);
extern void (*xroar_fullscreen_changed_cb)(int fullscreen);

/**************************************************************************/
/* Command line arguments */

/* Emulator interface */
extern char *xroar_opt_gl_filter;
#ifndef FAST_SOUND
extern int xroar_fast_sound;
#endif
extern int xroar_opt_fullscreen;
extern int xroar_frameskip;
extern char *xroar_opt_keymap;
extern char *xroar_opt_joy_left;
extern char *xroar_opt_joy_right;
extern int xroar_tapehack;
extern int xroar_trace_enabled;

/* Emulated machine */
extern char *xroar_opt_machine;
extern char *xroar_opt_bas;
extern char *xroar_opt_extbas;
extern char *xroar_opt_altbas;
extern int xroar_opt_nobas;
extern int xroar_opt_noextbas;
extern int xroar_opt_noaltbas;
extern char *xroar_opt_dostype;
extern char *xroar_opt_dos;
extern int xroar_opt_nodos;
extern int xroar_opt_tv;
extern int xroar_opt_ram;

/**************************************************************************/
/* Global flags */

/* NTSC cross-colour can either be rendered as a simple four colour palette,
 * or with a 5-bit lookup table */
#ifdef FAST_VDG
# define NUM_CROSS_COLOUR_RENDERERS (1)
#else
# define NUM_CROSS_COLOUR_RENDERERS (2)
#endif
#define CROSS_COLOUR_SIMPLE (0)
#define CROSS_COLOUR_5BIT   (1)
extern int xroar_cross_colour_renderer;
extern int xroar_noratelimit;

extern const char *xroar_rom_path;

#define UI_EVENT_LIST xroar_ui_events
#define MACHINE_EVENT_LIST xroar_machine_events
extern event_t *xroar_ui_events;
extern event_t *xroar_machine_events;

/**************************************************************************/

void xroar_getargs(int argc, char **argv);
int xroar_init(int argc, char **argv);
void xroar_shutdown(void);
void xroar_mainloop(void);
int xroar_filetype_by_ext(const char *filename);
int xroar_load_file_by_type(const char *filename, int autorun);

/* Helper functions */
void xroar_set_trace(int mode);
void xroar_new_disk(int drive);
void xroar_insert_disk(int drive);
void xroar_toggle_write_back(int drive);
void xroar_toggle_write_protect(int drive);
void xroar_cycle_cross_colour(void);
void xroar_quit(void);
void xroar_fullscreen(int action);
void xroar_load_file(const char **exts);
void xroar_run_file(const char **exts);
void xroar_cycle_keymap(void);
void xroar_set_machine(int machine);
void xroar_set_dos(int dos_type);
void xroar_save_snapshot(void);
void xroar_write_tape(void);
void xroar_hard_reset(void);
void xroar_soft_reset(void);

#endif  /* XROAR_XROAR_H_ */
