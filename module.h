/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2007  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef __MODULE_H__
#define __MODULE_H__

typedef struct {
	const char *name; 
	const char *description; 
	int (*init)(int argc, char **argv); 
	int initialised;
	void (*shutdown)(void);
	void (*helptext)(void);
} Module_Common;

typedef struct {
	Module_Common common;
	char *(*load_filename)(const char **extensions);
	char *(*save_filename)(const char **extensions);
} FileReqModule;

typedef struct {
	Module_Common common;
	void (*resize)(unsigned int w, unsigned int h);
	int (*set_fullscreen)(int fullscreen);
	int is_fullscreen;
	void (*vdg_vsync)(void);
	void (*vdg_set_mode)(unsigned int mode);
	void (*vdg_render_sg4)(void);
	void (*vdg_render_sg6)(void);
	void (*vdg_render_cg1)(void);
	void (*vdg_render_rg1)(void);
	void (*vdg_render_cg2)(void);
	void (*vdg_render_rg6)(void);
	void (*render_border)(void);
} VideoModule;

typedef struct {
	Module_Common common;
	void (*update)(void);
} SoundModule;

typedef struct {
	Module_Common common;
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
Module *module_select_by_arg(Module **list, const char *arg, int argc, char **argv);
Module *module_init(Module *module, int argc, char **argv);
Module *module_init_from_list(Module **list, Module *module, int argc, char **argv);
void module_shutdown(Module *module);
void module_helptext(Module *module);

#endif  /* __MODULE_H__ */
