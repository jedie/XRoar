/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MODULE_H_
#define XROAR_MODULE_H_

struct vdisk;

typedef struct {
	const char *name;
	const char *description;
	int (*init)(void);
	int initialised;
	void (*shutdown)(void);
} Module_Common;

typedef struct {
	Module_Common common;
	char *(*load_filename)(const char **extensions);
	char *(*save_filename)(const char **extensions);
} FileReqModule;

typedef struct {
	Module_Common common;
	void (*update_palette)(void);
	void (*reset)(void);
	void (*vsync)(void);
	void (*hsync)(void);
	void (*set_mode)(unsigned int mode);
	void (*render_border)(void);
	void (*resize)(unsigned int w, unsigned int h);
	int (*set_fullscreen)(int fullscreen);
#ifndef FAST_VDG
	void (*render_scanline)(uint8_t *vram_ptr, int beam_to);
#else
	void (*render_scanline)(uint8_t *vram_ptr);
#endif
	int is_fullscreen;
	float scale;
} VideoModule;

typedef struct {
	Module_Common common;
	void (*flush_frame)(void);
} SoundModule;

typedef struct {
	Module_Common common;
	void (*update_kbd_translate)(void);
} KeyboardModule;

typedef struct {
	Module_Common common;
} JoystickModule;

typedef struct {
	Module_Common common;
	FileReqModule **filereq_module_list;
	VideoModule **video_module_list;
	SoundModule **sound_module_list;
	KeyboardModule **keyboard_module_list;
	JoystickModule **joystick_module_list;
	void (*run)(void);
	void (*cross_colour_changed_cb)(int cc);
	void (*machine_changed_cb)(int machine_type);
	void (*cart_changed_cb)(int cart_index);
	void (*keymap_changed_cb)(int keymap);
	void (*fast_sound_changed_cb)(int fast);
	void (*input_tape_filename_cb)(const char *filename);
	void (*output_tape_filename_cb)(const char *filename);
	void (*update_tape_state)(int flags);  /* flag bits from tape.h */
	void (*update_drive_disk)(int drive, struct vdisk *disk);
	void (*update_drive_write_enable)(int drive, int write_enable);
	void (*update_drive_write_back)(int drive, int write_back);
} UIModule;

typedef union {
	Module_Common common;
	UIModule ui;
	FileReqModule filereq;
	VideoModule video;
	SoundModule sound;
	KeyboardModule keyboard;
	JoystickModule joystick;
} Module;

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

void module_print_list(Module **list);
Module *module_select(Module **list, const char *name);
Module *module_select_by_arg(Module **list, const char *name);
Module *module_init(Module *module);
Module *module_init_from_list(Module **list, Module *module);
void module_shutdown(Module *module);

#endif  /* XROAR_MODULE_H_ */
