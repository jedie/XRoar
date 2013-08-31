/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_JOYSTICK_H_
#define XROAR_JOYSTICK_H_

// Each joystick module contains a list of interfaces, with standard names:
//
// physical - interface reads from a real joystick
// keyboard - a keyboard module intercepts certain keys to fake a joystick
// mouse    - mouse position maps to joystick position

// Unlike other types of module, the joystick module list in a UI definition
// does not override the default, it supplements it.  Interfaces are searched
// for in both lists.  This allows both modules that can exist standalone and
// modules that require a specific active UI to be available.

// Specs are of the form [[MODULE:]INTERFACE:]CONTROL-SPEC.

// The CONTROL-SPEC will vary by interface:
//
// Interface    Axis spec                       Button spec
// phsical      DEVICE-NUMBER,AXIS-NUMBER       DEVICE-NUMER,BUTTON-NUMBER
// keyboard     KEY-NAME0,KEY-NAME1             KEY-NAME
// mouse        SCREEN0,SCREEN1                 BUTTON-NUMBER
//
// DEVICE-NUMBER - a physical joystick index, order will depend on the OS
// AXIS-NUMBER, BUTTON-NUMBER - index of relevant control on device
// 0,1 - Push (left,up) or (right,down)
// KEY-NAME - will (currently) depend on the underlying toolkit
// SCREEN - coordinates define bounding box for mouse-to-joystick mapping

#define JOYSTICK_NUM_PORTS (2)
#define JOYSTICK_NUM_AXES (2)
#define JOYSTICK_NUM_BUTTONS (2)

struct joystick_config {
	char *name;
	char *description;
	unsigned index;
	char *axis_specs[JOYSTICK_NUM_AXES];
	char *button_specs[JOYSTICK_NUM_BUTTONS];
};

extern struct joystick_config *joystick_port_config[JOYSTICK_NUM_PORTS];

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Interfaces will define one of these for reading each control, plus
// associated context data:
typedef unsigned (*js_read_axis_func)(void *);
typedef _Bool (*js_read_button_func)(void *);

struct joystick_interface;

struct joystick_axis {
	js_read_axis_func read;
	void *data;
	struct joystick_interface *interface;
};

struct joystick_button {
	js_read_button_func read;
	void *data;
	struct joystick_interface *interface;
};

struct joystick_interface {
	const char *name;
	struct joystick_axis *(*configure_axis)(char *spec, unsigned jaxis);
	struct joystick_button *(*configure_button)(char *spec, unsigned jbutton);
	void (*unmap_axis)(struct joystick_axis *);
	void (*unmap_button)(struct joystick_button *);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct joystick_config *joystick_config_new(void);
unsigned joystick_config_count(void);
struct joystick_config *joystick_config_index(unsigned i);
struct joystick_config *joystick_config_by_name(const char *name);

void joystick_init(void);
void joystick_shutdown(void);

void joystick_map(struct joystick_config *, unsigned port);
void joystick_unmap(unsigned port);
void joystick_set_virtual(struct joystick_config *);
void joystick_swap(void);
void joystick_cycle(void);

int joystick_read_axis(int port, int axis);
int joystick_read_buttons(void);

#endif  /* XROAR_JOYSTICK_H_ */
