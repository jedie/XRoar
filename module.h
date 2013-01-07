/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MODULE_H_
#define XROAR_MODULE_H_

#include <inttypes.h>

struct vdisk;

struct module {
	const char *name;
	const char *description;
	int (*init)(void);
	_Bool initialised;
	void (*shutdown)(void);
};

typedef struct {
	struct module common;
	char *(*load_filename)(const char **extensions);
	char *(*save_filename)(const char **extensions);
} FileReqModule;

typedef struct {
	struct module common;
	void (*update_palette)(void);
	void (*resize)(unsigned int w, unsigned int h);
	int (*set_fullscreen)(_Bool fullscreen);
	_Bool is_fullscreen;
	float scale;
	void (*render_scanline)(uint8_t *scanline_data);
	void (*vsync)(void);
	void (*update_cross_colour_phase)(void);
} VideoModule;

typedef struct {
	struct module common;
	void (*flush_frame)(void *buffer);
} SoundModule;

typedef struct {
	struct module common;
	void (*update_kbd_translate)(void);
} KeyboardModule;

typedef struct {
	struct module common;
} JoystickModule;

typedef struct {
	struct module common;
	FileReqModule **filereq_module_list;
	VideoModule **video_module_list;
	SoundModule **sound_module_list;
	KeyboardModule **keyboard_module_list;
	JoystickModule **joystick_module_list;
	void (*run)(void);
	void (*cross_colour_changed_cb)(int cc);
	void (*machine_changed_cb)(int machine_type);
	void (*cart_changed_cb)(int cart_index);
	void (*keymap_changed_cb)(int map);
	void (*fast_sound_changed_cb)(_Bool fast);
	void (*input_tape_filename_cb)(const char *filename);
	void (*output_tape_filename_cb)(const char *filename);
	void (*update_tape_state)(int flags);  /* flag bits from tape.h */
	void (*update_drive_disk)(int drive, struct vdisk *disk);
	void (*update_drive_write_enable)(int drive, _Bool write_enable);
	void (*update_drive_write_back)(int drive, _Bool write_back);
} UIModule;

extern UIModule **ui_module_list;
extern UIModule *ui_module;
extern FileReqModule **filereq_module_list;
extern FileReqModule *filereq_module;
extern VideoModule **video_module_list;
extern VideoModule *video_module;
extern SoundModule **sound_module_list;
extern SoundModule *sound_module;
extern KeyboardModule **keyboard_module_list;
extern KeyboardModule *keyboard_module;
extern JoystickModule **joystick_module_list;
extern JoystickModule *joystick_module;

void module_print_list(struct module **list);
struct module *module_select(struct module **list, const char *name);
struct module *module_select_by_arg(struct module **list, const char *name);
struct module *module_init(struct module *module);
struct module *module_init_from_list(struct module **list, struct module *module);
void module_shutdown(struct module *module);

#endif  /* XROAR_MODULE_H_ */
