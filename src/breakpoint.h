/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_BREAKPOINT_H_
#define XROAR_BREAKPOINT_H_

/*
 * Breakpoint support both for internal hooks and user-added traps (e.g. via
 * the GDB target).
 *
 * For internal hooks, an array of struct breakpoint is usually supplied using
 * bp_add_list, and the add_cond field determines whether each breakpoint is
 * relevant to the currently configured architecture.
 *
 * Once a breakpoint is added, match_mask determines the match bits that need
 * to match to trigger, and match_cond specifies what those values must be.
 */

/* Conditions that must be met for breakpoints to be considered: */

#define BP_MACHINE_ARCH (1 << 0)
#define BP_CRC_BAS (1 << 1)
#define BP_CRC_EXT (1 << 2)
#define BP_CRC_COMBINED (1 << 3)

/* Match conditions. */

#define BP_SAM_TY (1 << 15)
#define BP_SAM_P1 (1 << 10)

/* Useful mask and condition combinations. */

#define BP_MASK_ROM (BP_SAM_TY)
#define BP_COND_ROM (0)

typedef void (*bp_handler)(void *);

struct breakpoint {
	// Dynamic match conditions
	unsigned match_mask;
	unsigned match_cond;
	unsigned address;
	unsigned address_end;

	// Handler
	bp_handler handler;
	void *handler_data;

	// Conditions for bp_list_add to consider entry for adding
	unsigned add_cond;
	int cond_machine_arch;
	const char *cond_crc_combined;
	const char *cond_crc_bas;
	const char *cond_crc_extbas;
	const char *cond_crc_altbas;
};

// Chosen to match up to the GDB protcol watchpoint type minus 1.
#define WP_WRITE (1)
#define WP_READ  (2)
#define WP_BOTH  (3)

/* Convenience macros for standard types of breakpoint. */

#define BP_DRAGON64_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_COMBINED, .cond_crc_combined = "@d64_1", __VA_ARGS__ }
#define BP_DRAGON32_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_COMBINED, .cond_crc_combined = "@d32", __VA_ARGS__ }
#define BP_DRAGON_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_COMBINED, .cond_crc_combined = "@dragon", __VA_ARGS__ }

#define BP_COCO_BAS10_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@bas10", __VA_ARGS__ }
#define BP_COCO_BAS11_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@bas11", __VA_ARGS__ }
#define BP_COCO_BAS12_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@bas12", __VA_ARGS__ }
#define BP_COCO_BAS13_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@bas13", __VA_ARGS__ }
#define BP_MX1600_BAS_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@mx1600", __VA_ARGS__ }
#define BP_COCO_ROM(...) \
	{ .match_mask = BP_MASK_ROM, .match_cond = BP_COND_ROM, .add_cond = BP_CRC_BAS, .cond_crc_bas = "@coco", __VA_ARGS__ }

#define bp_add_list(bp) bp_add_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))
#define bp_remove_list(bp) bp_remove_n(bp, (sizeof(bp) / sizeof(struct breakpoint)))

void bp_add(struct breakpoint *bp);
void bp_add_n(struct breakpoint *bp, int n);

void bp_remove(struct breakpoint *bp);
void bp_remove_n(struct breakpoint *bp, int n);

// Manipulate simple traps.  xroar_machine_trap() used as a handler.

void bp_hbreak_add(unsigned addr, unsigned match_mask, unsigned match_cond);
void bp_hbreak_remove(unsigned addr, unsigned match_mask, unsigned match_cond);

void bp_wp_add(unsigned type, unsigned addr, unsigned nbytes, unsigned match_mask, unsigned match_cond);
void bp_wp_remove(unsigned type, unsigned addr, unsigned nbytes, unsigned match_mask, unsigned match_cond);

void bp_wp_read_hook(unsigned address);
void bp_wp_write_hook(unsigned address);

#endif  /* XROAR_BREAKPOINT_H_ */
