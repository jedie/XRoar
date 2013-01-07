/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MACHINE_H_
#define XROAR_MACHINE_H_

#include <inttypes.h>
#include <sys/types.h>

#include "mc6821.h"
#include "mc6809.h"

struct cart_config;
struct cart;

/* Dragon 64s and later Dragon 32s used a 14.218MHz crystal
 * (ref: "Dragon 64 differences", Graham E. Kinns and then a motherboard) */
/* #define OSCILLATOR_RATE 14218000 */
#define OSCILLATOR_RATE 14318180

#define RESET_SOFT 0
#define RESET_HARD 1

#define IS_DRAGON64 (xroar_machine_config->architecture == ARCH_DRAGON64)
#define IS_DRAGON32 (xroar_machine_config->architecture == ARCH_DRAGON32)
#define IS_DRAGON (!IS_COCO)
#define IS_COCO (xroar_machine_config->architecture == ARCH_COCO)

#define IS_PAL (xroar_machine_config->tv_standard == TV_PAL)
#define IS_NTSC (xroar_machine_config->tv_standard == TV_NTSC)

#define ANY_AUTO (-1)
#define MACHINE_DRAGON32 (0)
#define MACHINE_DRAGON64 (1)
#define MACHINE_TANO     (2)
#define MACHINE_COCO     (3)
#define MACHINE_COCOUS   (4)
#define ARCH_DRAGON32 (0)
#define ARCH_DRAGON64 (1)
#define ARCH_COCO     (2)
#define CPU_MC6809 (0)
#define CPU_HD6309 (1)
#define ROMSET_DRAGON32 (0)
#define ROMSET_DRAGON64 (1)
#define ROMSET_COCO     (2)
#define TV_PAL  (0)
#define TV_NTSC (1)

/* These are now purely for backwards-compatibility with old snapshots.
 * Cartridge types are now more generic: see cart.h.  */
#define DOS_NONE      (0)
#define DOS_DRAGONDOS (1)
#define DOS_RSDOS     (2)
#define DOS_DELTADOS  (3)

/* NTSC cross-colour can either be switched off, or sychronised to one
 * of two phases (a real CoCo does not emit a colour burst in high resolution
 * mode, so NTSC televisions sync to one at random on machine reset) */
#define NUM_CROSS_COLOUR_PHASES (3)
#define CROSS_COLOUR_OFF  (0)
#define CROSS_COLOUR_KBRW (1)
#define CROSS_COLOUR_KRBW (2)

struct machine_config {
	char *name;
	char *description;
	int index;
	int architecture;
	int cpu;
	char *vdg_palette;
	int keymap;
	int tv_standard;
	int cross_colour_phase;
	int ram;
	_Bool nobas;
	_Bool noextbas;
	_Bool noaltbas;
	char *bas_rom;
	char *extbas_rom;
	char *altbas_rom;
};

extern unsigned int machine_ram_size;  /* RAM in bytes, up to 64K */
extern uint8_t machine_ram[0x10000];
extern struct MC6809 *CPU0;
extern struct MC6821 PIA0, PIA1;
extern struct cart *machine_cart;
extern _Bool has_bas, has_extbas, has_altbas;
extern uint32_t crc_bas, crc_extbas, crc_altbas;

/* Add a new machine config: */
struct machine_config *machine_config_new(void);
/* For finding known configs: */
int machine_config_count(void);
struct machine_config *machine_config_index(int i);
struct machine_config *machine_config_by_name(const char *name);
struct machine_config *machine_config_by_arch(int arch);
/* Find a working machine by searching available ROMs: */
struct machine_config *machine_config_first_working(void);
/* Complete a config replacing ANY_AUTO entries: */
void machine_config_complete(struct machine_config *mc);

void machine_init(void);
void machine_shutdown(void);
void machine_configure(struct machine_config *mc);  /* apply config */
void machine_reset(_Bool hard);
void machine_run(int ncycles);

/* simplified read & write byte for convenience functions */
uint8_t machine_read_byte(uint16_t A);
void machine_write_byte(uint16_t A, uint8_t D);
/* simulate an RTS without otherwise affecting machine state */
void machine_op_rts(struct MC6809 *cpu);

void machine_set_fast_sound(_Bool fast);
void machine_select_fast_sound(_Bool fast);
void machine_update_sound(void);

void machine_insert_cart(struct cart_config *cc);
void machine_remove_cart(void);

int machine_load_rom(const char *path, uint8_t *dest, size_t max_size);

#endif  /* XROAR_MACHINE_H_ */
