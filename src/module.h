/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MODULE_H_
#define XROAR_MODULE_H_

#include <stdint.h>

struct joystick_module;
struct vdisk;

struct module {
	const char *name;
	const char *description;
	_Bool (* const init)(void);
	_Bool initialised;
	void (* const shutdown)(void);
};

typedef struct {
	struct module common;
	char *(* const load_filename)(const char **extensions);
	char *(* const save_filename)(const char **extensions);
} FileReqModule;

typedef struct {
	struct module common;
	int scanline;
	int window_x, window_y;
	int window_w, window_h;
	void (* const update_palette)(void);
	void (* const resize)(unsigned int w, unsigned int h);
	int (* const set_fullscreen)(_Bool fullscreen);
	_Bool is_fullscreen;
	void (*render_scanline)(uint8_t *scanline_data);
	void (* const vsync)(void);
	void (* const refresh)(void);
	void (* const update_cross_colour_phase)(void);
} VideoModule;

typedef struct {
	struct module common;
	void *(* const write_buffer)(void *buffer);
} SoundModule;

typedef struct {
	struct module common;
	void (* const update_kbd_translate)(void);
} KeyboardModule;

typedef struct {
	struct module common;
	FileReqModule * const *filereq_module_list;
	VideoModule * const *video_module_list;
	SoundModule * const *sound_module_list;
	KeyboardModule * const *keyboard_module_list;
	struct joystick_module * const *joystick_module_list;
	void (* const run)(void);
	void (* const fullscreen_changed_cb)(_Bool fullscreen);
	void (* const cross_colour_changed_cb)(int cc);
	void (* const vdg_inverse_cb)(_Bool inverse);
	void (* const machine_changed_cb)(int machine_type);
	void (* const cart_changed_cb)(int cart_index);
	void (* const keymap_changed_cb)(int map);
	void (* const joystick_changed_cb)(int port, const char *name);
	void (* const kbd_translate_changed_cb)(_Bool translate);
	void (* const fast_sound_changed_cb)(_Bool fast);
	void (* const input_tape_filename_cb)(const char *filename);
	void (* const output_tape_filename_cb)(const char *filename);
	void (* const update_tape_state)(int flags);  /* flag bits from tape.h */
	void (* const update_drive_disk)(int drive, struct vdisk *disk);
	void (* const update_drive_write_enable)(int drive, _Bool write_enable);
	void (* const update_drive_write_back)(int drive, _Bool write_back);
} UIModule;

extern UIModule * const *ui_module_list;
extern UIModule *ui_module;
extern FileReqModule * const *filereq_module_list;
extern FileReqModule *filereq_module;
extern VideoModule * const *video_module_list;
extern VideoModule *video_module;
extern SoundModule * const *sound_module_list;
extern SoundModule *sound_module;
extern KeyboardModule * const *keyboard_module_list;
extern KeyboardModule *keyboard_module;

void module_print_list(struct module **list);
struct module *module_select(struct module **list, const char *name);
struct module *module_select_by_arg(struct module **list, const char *name);
struct module *module_init(struct module *module);
struct module *module_init_from_list(struct module **list, struct module *module);
void module_shutdown(struct module *module);

#endif  /* XROAR_MODULE_H_ */
