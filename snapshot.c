/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2006  Ciaran Anscomb
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>

#include "xroar.h"
#include "machine.h"
#include "m6809.h"
#include "sam.h"
#include "vdg.h"
#include "pia.h"
#include "snapshot.h"
#include "tape.h"
#include "logging.h"
#include "fs.h"
#include "ui.h"
#include "types.h"

/* Write files in 'chunks', each with an identifying byte and a 16-bit
 * length.  This should mean no changes are required that break the
 * format.  */

#define ID_REGISTER_DUMP (0)	/* deprecated */
#define ID_RAM_PAGE0     (1)
#define ID_PIA_REGISTERS (2)
#define ID_SAM_REGISTERS (3)
#define ID_M6809_STATE   (4)
#define ID_KEYBOARD_MAP  (5)
#define ID_ARCHITECTURE  (6)
#define ID_RAM_PAGE1     (7)
#define ID_MACHINECONFIG (8)
#define ID_SNAPVERSION   (9)

#define SNAPSHOT_VERSION_MAJOR 1
#define SNAPSHOT_VERSION_MINOR 2

int write_snapshot(char *filename) {
	int fd;
	M6809State cpu_state;
	if ((fd = fs_open(filename, FS_WRITE)) == -1)
		return -1;
	fs_write(fd, "XRoar snapshot.\012\000", 17);
	/* Snapshot version */
	fs_write_byte(fd, ID_SNAPVERSION); fs_write_word16(fd, 3);
	fs_write_byte(fd, SNAPSHOT_VERSION_MAJOR);
	fs_write_word16(fd, SNAPSHOT_VERSION_MINOR);
	/* Machine running config */
	fs_write_byte(fd, ID_MACHINECONFIG); fs_write_word16(fd, 7);
	fs_write_byte(fd, running_machine);
	fs_write_byte(fd, running_config.architecture);
	fs_write_byte(fd, running_config.romset);
	fs_write_byte(fd, running_config.keymap);
	fs_write_byte(fd, running_config.tv_standard);
	fs_write_byte(fd, running_config.ram);
	fs_write_byte(fd, running_config.dos_type);
	/* RAM page 0 */
	fs_write_byte(fd, ID_RAM_PAGE0); fs_write_word16(fd, machine_page0_ram);
	fs_write(fd, ram0, machine_page0_ram);
	/* RAM page 1 */
	if (machine_page1_ram > 0) {
		fs_write_byte(fd, ID_RAM_PAGE1);
		fs_write_word16(fd, machine_page1_ram);
		fs_write(fd, ram1, machine_page1_ram);
	}
	/* PIA state written before CPU state because PIA may have
	 * unacknowledged interrupts pending already cleared in the CPU
	 * state */
	fs_write_byte(fd, ID_PIA_REGISTERS); fs_write_word16(fd, 3*4);
	/* PIA_0A */
	fs_write_byte(fd, PIA_0A.direction_register);
	fs_write_byte(fd, PIA_0A.output_register);
	fs_write_byte(fd, PIA_0A.control_register);
	/* PIA_0B */
	fs_write_byte(fd, PIA_0B.direction_register);
	fs_write_byte(fd, PIA_0B.output_register);
	fs_write_byte(fd, PIA_0B.control_register);
	/* PIA_1A */
	fs_write_byte(fd, PIA_1A.direction_register);
	fs_write_byte(fd, PIA_1A.output_register);
	fs_write_byte(fd, PIA_1A.control_register);
	/* PIA_1B */
	fs_write_byte(fd, PIA_1B.direction_register);
	fs_write_byte(fd, PIA_1B.output_register);
	fs_write_byte(fd, PIA_1B.control_register);
	/* M6809 state */
	fs_write_byte(fd, ID_M6809_STATE); fs_write_word16(fd, 21);
	m6809_get_state(&cpu_state);
	fs_write_byte(fd, cpu_state.reg_cc);
	fs_write_byte(fd, cpu_state.reg_a);
	fs_write_byte(fd, cpu_state.reg_b);
	fs_write_byte(fd, cpu_state.reg_dp);
	fs_write_word16(fd, cpu_state.reg_x);
	fs_write_word16(fd, cpu_state.reg_y);
	fs_write_word16(fd, cpu_state.reg_u);
	fs_write_word16(fd, cpu_state.reg_s);
	fs_write_word16(fd, cpu_state.reg_pc);
	fs_write_byte(fd, cpu_state.halt);
	fs_write_byte(fd, cpu_state.nmi);
	fs_write_byte(fd, cpu_state.firq);
	fs_write_byte(fd, cpu_state.irq);
	fs_write_byte(fd, cpu_state.wait_for_interrupt);
	fs_write_byte(fd, cpu_state.skip_register_push);
	fs_write_byte(fd, cpu_state.nmi_armed);
	/* SAM */
	fs_write_byte(fd, ID_SAM_REGISTERS); fs_write_word16(fd, 2);
	fs_write_word16(fd, sam_register);
	fs_close(fd);
	return 0;
}

static int old_arch_mapping[4] = {
	MACHINE_DRAGON32,
	MACHINE_DRAGON64,
	MACHINE_TANO,
	MACHINE_COCOUS
};

int read_snapshot(char *filename) {
	int fd;
	uint8_t buffer[17];
	uint8_t section, tmp8;
	uint16_t size, tmp16;
	M6809State cpu_state;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	fs_read(fd, buffer, 17);
	if (strncmp((char *)buffer, "XRoar snapshot.\012\000", 17)) {
		/* Very old-style snapshot.  Register dump always came first.
		 * Also, it used to be written out as only taking 12 bytes. */
		if (buffer[0] != ID_REGISTER_DUMP || buffer[1] != 0
				|| (buffer[2] != 12 && buffer[2] != 14)) {
			fs_close(fd);
			return -1;
		}
		m6809_set_registers(buffer + 3);
	}
	/* Default to Dragon 64 for old snapshots */
	requested_machine = MACHINE_DRAGON64;
	machine_clear_requested_config();
	/* Need reset in case old snapshot doesn't trigger one */
	machine_reset(RESET_HARD);
	while (fs_read_byte(fd, &section) > 0) {
		fs_read_word16(fd, &size);
		switch (section) {
			case ID_ARCHITECTURE:
				/* Deprecated: Machine architecture */
				fs_read_byte(fd, &tmp8);
				tmp8 %= 4;
				requested_machine = old_arch_mapping[tmp8];
				machine_reset(RESET_HARD);
				break;
			case ID_KEYBOARD_MAP:
				/* Deprecated: Keyboard map */
				fs_read_byte(fd, &tmp8);
				requested_config.keymap = tmp8;
				machine_set_keymap(tmp8);
				break;
			case ID_REGISTER_DUMP:
				/* Deprecated */
				fs_read(fd, buffer, 14);
				m6809_set_registers(buffer);
				break;
			case ID_M6809_STATE:
				/* M6809 state */
				fs_read_byte(fd, &cpu_state.reg_cc);
				fs_read_byte(fd, &cpu_state.reg_a);
				fs_read_byte(fd, &cpu_state.reg_b);
				fs_read_byte(fd, &cpu_state.reg_dp);
				fs_read_word16(fd, &cpu_state.reg_x);
				fs_read_word16(fd, &cpu_state.reg_y);
				fs_read_word16(fd, &cpu_state.reg_u);
				fs_read_word16(fd, &cpu_state.reg_s);
				fs_read_word16(fd, &cpu_state.reg_pc);
				fs_read_byte(fd, &cpu_state.halt);
				fs_read_byte(fd, &cpu_state.nmi);
				fs_read_byte(fd, &cpu_state.firq);
				fs_read_byte(fd, &cpu_state.irq);
				fs_read_byte(fd, &cpu_state.wait_for_interrupt);
				fs_read_byte(fd, &cpu_state.skip_register_push);
				fs_read_byte(fd, &cpu_state.nmi_armed);
				cpu_state.firq &= 3;
				cpu_state.irq &= 3;
				m6809_set_state(&cpu_state);
				break;
			case ID_MACHINECONFIG:
				/* Machine running config */
				fs_read_byte(fd, &tmp8);
				requested_machine = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.architecture = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.romset = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.keymap = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.tv_standard = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.ram = tmp8;
				fs_read_byte(fd, &tmp8);
				requested_config.dos_type = tmp8;
				machine_reset(RESET_HARD);
				break;
			case ID_PIA_REGISTERS:
				/* PIA_0A */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_DDR(PIA_0A);
				PIA_WRITE_P0DA(tmp8); /* DDR */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_PDR(PIA_0A);
				PIA_WRITE_P0DA(tmp8); /* OR */
				fs_read_byte(fd, &tmp8);
				PIA_WRITE_P0CA(tmp8);  /* CR */
				/* PIA_0B */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_DDR(PIA_0B);
				PIA_WRITE_P0DB(tmp8);  /* DDR */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_PDR(PIA_0B);
				PIA_WRITE_P0DB(tmp8);  /* OR */
				fs_read_byte(fd, &tmp8);
				PIA_WRITE_P0CB(tmp8);  /* CR */
				/* PIA_1A */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_DDR(PIA_1A);
				PIA_WRITE_P1DA(tmp8);  /* DDR */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_PDR(PIA_1A);
				PIA_WRITE_P1DA(tmp8);  /* OR */
				fs_read_byte(fd, &tmp8);
				PIA_WRITE_P1CA(tmp8);  /* CR */
				/* PIA_1B */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_DDR(PIA_1B);
				PIA_WRITE_P1DB(tmp8);  /* DDR */
				fs_read_byte(fd, &tmp8);
				PIA_SELECT_PDR(PIA_1B);
				PIA_WRITE_P1DB(tmp8);  /* OR */
				fs_read_byte(fd, &tmp8);
				PIA_WRITE_P1CB(tmp8);  /* CR */
				break;
			case ID_RAM_PAGE0:
				fs_read(fd, ram0, size);
				break;
			case ID_RAM_PAGE1:
				fs_read(fd, ram1, size);
				break;
			case ID_SAM_REGISTERS:
				/* SAM */
				fs_read_word16(fd, &tmp16);
				sam_register = tmp16;
				sam_update_from_register();
				break;
			case ID_SNAPVERSION:
				/* Snapshot version - abort if snapshot
				 * contains stuff we don't understand */
				fs_read_byte(fd, &tmp8);
				fs_read_word16(fd, &tmp16);
				if (tmp8 != SNAPSHOT_VERSION_MAJOR || tmp16 > SNAPSHOT_VERSION_MINOR) {
					fs_close(fd);
					return -1;
				}
				break;
			default:
				/* Unknown chunk - skip it */
				for (; size; size--)
					fs_read_byte(fd, &tmp8);
				break;
		}
	}
	fs_close(fd);
	return 0;
}
