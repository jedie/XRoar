/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2010  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MACHINE_H_
#define XROAR_MACHINE_H_

#include "types.h"
#include "cart.h"
#include "mc6821.h"

/* Dragon 64s and later Dragon 32s used a 14.218MHz crystal
 * (ref: "Dragon 64 differences", Graham E. Kinns and then a motherboard) */
//#define OSCILLATOR_RATE 14218000
#define OSCILLATOR_RATE 14318180

#define CPU_SLOW_DIVISOR 16
#define CPU_FAST_DIVISOR 8

#define IS_DRAGON64 (running_config.architecture == ARCH_DRAGON64)
#define IS_DRAGON32 (running_config.architecture == ARCH_DRAGON32)
#define IS_DRAGON (!IS_COCO)
#define IS_COCO (running_config.architecture == ARCH_COCO)

#define IS_PAL (running_config.tv_standard == TV_PAL)
#define IS_NTSC (running_config.tv_standard == TV_NTSC)

#define DOS_ENABLED (running_config.dos_type != DOS_NONE)
#define IS_DRAGONDOS (running_config.dos_type == DOS_DRAGONDOS)
#define IS_RSDOS (running_config.dos_type == DOS_RSDOS)
#define IS_DELTADOS (running_config.dos_type == DOS_DELTADOS)

#define ANY_AUTO (-1)
#define MACHINE_DRAGON32 (0)
#define MACHINE_DRAGON64 (1)
#define MACHINE_TANO     (2)
#define MACHINE_COCO     (3)
#define MACHINE_COCOUS   (4)
#define ARCH_DRAGON32 (0)
#define ARCH_DRAGON64 (1)
#define ARCH_COCO     (2)
#define ROMSET_DRAGON32 (0)
#define ROMSET_DRAGON64 (1)
#define ROMSET_COCO     (2)
#define TV_PAL  (0)
#define TV_NTSC (1)
#define DOS_NONE      (0)
#define DOS_DRAGONDOS (1)
#define DOS_RSDOS     (2)
#define DOS_DELTADOS  (3)

#define NUM_MACHINE_TYPES (5)
#define NUM_ARCHITECTURES (3)
#define NUM_ROMSETS       (3)
#define NUM_TV_STANDARDS  (2)
#define NUM_DOS_TYPES     (4)

/* NTSC cross-colour can either be switched off, or sychronised to one
 * of two phases (a real CoCo does not emit a colour burst in high resolution
 * mode, so NTSC televisions sync to one at random on machine reset) */
#define NUM_CROSS_COLOUR_PHASES (3)
#define CROSS_COLOUR_OFF  (0)
#define CROSS_COLOUR_KBRW (1)
#define CROSS_COLOUR_KRBW (2)

typedef struct {
	int architecture;
	int romset;
	int keymap;
	int tv_standard;
	int cross_colour_phase;
	int ram;
	int dos_type;
	const char *bas_rom;
	const char *extbas_rom;
	const char *altbas_rom;
	const char *dos_rom;
} MachineConfig;

extern const char *machine_names[NUM_MACHINE_TYPES];
extern const char *dos_type_names[NUM_DOS_TYPES];
extern MachineConfig machine_defaults[NUM_MACHINE_TYPES];
extern int requested_machine;
extern int running_machine;
extern MachineConfig requested_config;
extern MachineConfig running_config;

extern unsigned int machine_ram_size;  /* RAM in bytes, up to 64K */
extern uint8_t ram0[0x10000];
extern uint8_t rom0[0x4000];
extern uint8_t rom1[0x4000];
extern MC6821_PIA PIA0, PIA1;
extern struct cart *machine_cart;

extern Cycle current_cycle;
extern int noextbas;

void machine_getargs(void);
void machine_init(void);
void machine_shutdown(void);
void machine_reset(int hard);

void machine_clear_requested_config(void);
void machine_insert_cart(struct cart *cart);
void machine_remove_cart(void);

int machine_load_rom(const char *path, uint8_t *dest, size_t max_size);

#endif  /* XROAR_MACHINE_H_ */
