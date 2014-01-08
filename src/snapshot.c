/*  Copyright 2003-2014 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

#include "cart.h"
#include "fs.h"
#include "keyboard.h"
#include "hd6309.h"
#include "logging.h"
#include "machine.h"
#include "mc6809.h"
#include "mc6821.h"
#include "mc6847.h"
#include "sam.h"
#include "snapshot.h"
#include "tape.h"
#include "vdisk.h"
#include "vdrive.h"
#include "xroar.h"

/* Write files in 'chunks', each with an identifying byte and a 16-bit length.
 * This should mean no changes are required that break the format.  */

/* Note: Setting up the correct ROM select for Dragon 64 depends on SAM
 * register update following PIA configuration. */

#define ID_REGISTER_DUMP (0)  // deprecated - part of ID_MC6809_STATE
#define ID_RAM_PAGE0     (1)
#define ID_PIA_REGISTERS (2)
#define ID_SAM_REGISTERS (3)
#define ID_MC6809_STATE  (4)
#define ID_KEYBOARD_MAP  (5)  // deprecated - part of ID_MACHINECONFIG
#define ID_ARCHITECTURE  (6)  // deprecated - part of ID_MACHINECONFIG
#define ID_RAM_PAGE1     (7)
#define ID_MACHINECONFIG (8)
#define ID_SNAPVERSION   (9)
#define ID_VDISK_FILE    (10)
#define ID_HD6309_STATE  (11)

#define SNAPSHOT_VERSION_MAJOR 1
#define SNAPSHOT_VERSION_MINOR 7

static void write_chunk_header(FILE *fd, unsigned id, unsigned size) {
	fs_write_uint8(fd, id);
	fs_write_uint16(fd, size);
}

static void write_mc6809(FILE *fd, struct MC6809 *cpu) {
	write_chunk_header(fd, ID_MC6809_STATE, 20);
	fs_write_uint8(fd, cpu->reg_cc);
	fs_write_uint8(fd, MC6809_REG_A(cpu));
	fs_write_uint8(fd, MC6809_REG_B(cpu));
	fs_write_uint8(fd, cpu->reg_dp);
	fs_write_uint16(fd, cpu->reg_x);
	fs_write_uint16(fd, cpu->reg_y);
	fs_write_uint16(fd, cpu->reg_u);
	fs_write_uint16(fd, cpu->reg_s);
	fs_write_uint16(fd, cpu->reg_pc);
	fs_write_uint8(fd, cpu->halt);
	fs_write_uint8(fd, cpu->nmi);
	fs_write_uint8(fd, cpu->firq);
	fs_write_uint8(fd, cpu->irq);
	fs_write_uint8(fd, cpu->state);
	fs_write_uint8(fd, cpu->nmi_armed);
}

static uint8_t tfm_reg(struct HD6309 *hcpu, uint16_t *ptr) {
	struct MC6809 *cpu = &hcpu->mc6809;
	if (ptr == &cpu->reg_d)
		return 0;
	if (ptr == &cpu->reg_x)
		return 1;
	if (ptr == &cpu->reg_y)
		return 2;
	if (ptr == &cpu->reg_u)
		return 3;
	if (ptr == &cpu->reg_s)
		return 4;
	return 15;
}

static void write_hd6309(FILE *fd, struct HD6309 *hcpu) {
	struct MC6809 *cpu = &hcpu->mc6809;
	write_chunk_header(fd, ID_HD6309_STATE, 27);
	fs_write_uint8(fd, cpu->reg_cc);
	fs_write_uint8(fd, MC6809_REG_A(cpu));
	fs_write_uint8(fd, MC6809_REG_B(cpu));
	fs_write_uint8(fd, cpu->reg_dp);
	fs_write_uint16(fd, cpu->reg_x);
	fs_write_uint16(fd, cpu->reg_y);
	fs_write_uint16(fd, cpu->reg_u);
	fs_write_uint16(fd, cpu->reg_s);
	fs_write_uint16(fd, cpu->reg_pc);
	fs_write_uint8(fd, cpu->halt);
	fs_write_uint8(fd, cpu->nmi);
	fs_write_uint8(fd, cpu->firq);
	fs_write_uint8(fd, cpu->irq);
	fs_write_uint8(fd, hcpu->state);  // 6309-specific
	fs_write_uint8(fd, cpu->nmi_armed);
	// 6309-specific extras:
	fs_write_uint8(fd, HD6309_REG_E(hcpu));
	fs_write_uint8(fd, HD6309_REG_F(hcpu));
	fs_write_uint16(fd, hcpu->reg_v);
	fs_write_uint8(fd, hcpu->reg_md);
	uint8_t tfm_src_dest = tfm_reg(hcpu, hcpu->tfm_src) << 4;
	tfm_src_dest |= tfm_reg(hcpu, hcpu->tfm_dest);
	fs_write_uint8(fd, tfm_src_dest);
	// lowest 4 bits of each of these is enough:
	uint8_t tfm_mod = (hcpu->tfm_src_mod & 15) << 4;
	tfm_mod |= (hcpu->tfm_dest_mod & 15);
	fs_write_uint8(fd, tfm_mod);
}

int write_snapshot(const char *filename) {
	FILE *fd;
	if (!(fd = fopen(filename, "wb")))
		return -1;
	fwrite("XRoar snapshot.\012\000", 17, 1, fd);
	// Snapshot version
	write_chunk_header(fd, ID_SNAPVERSION, 3);
	fs_write_uint8(fd, SNAPSHOT_VERSION_MAJOR);
	fs_write_uint16(fd, SNAPSHOT_VERSION_MINOR);
	// Machine running config
	write_chunk_header(fd, ID_MACHINECONFIG, 8);
	fs_write_uint8(fd, xroar_machine_config->index);
	fs_write_uint8(fd, xroar_machine_config->architecture);
	fs_write_uint8(fd, xroar_machine_config->cpu);
	fs_write_uint8(fd, xroar_machine_config->keymap);
	fs_write_uint8(fd, xroar_machine_config->tv_standard);
	fs_write_uint8(fd, xroar_machine_config->ram);
	if (xroar_cart) {
		fs_write_uint8(fd, xroar_cart->config->type);
	} else {
		fs_write_uint8(fd, 0);
	}
	fs_write_uint8(fd, xroar_machine_config->cross_colour_phase);
	// RAM page 0
	unsigned ram0_size = machine_ram_size > 0x8000 ? 0x8000 : machine_ram_size;
	write_chunk_header(fd, ID_RAM_PAGE0, ram0_size);
	fwrite(machine_ram, 1, ram0_size, fd);
	// RAM page 1
	if (machine_ram_size > 0x8000) {
		unsigned ram1_size = machine_ram_size - 0x8000;
		write_chunk_header(fd, ID_RAM_PAGE1, ram1_size);
		fwrite(machine_ram + 0x8000, 1, ram1_size, fd);
	}
	// PIA state written before CPU state because PIA may have
	// unacknowledged interrupts pending already cleared in the CPU state
	write_chunk_header(fd, ID_PIA_REGISTERS, 3 * 4);
	for (int i = 0; i < 2; i++) {
		struct MC6821 *pia = machine_get_pia(i);
		fs_write_uint8(fd, pia->a.direction_register);
		fs_write_uint8(fd, pia->a.output_register);
		fs_write_uint8(fd, pia->a.control_register);
		fs_write_uint8(fd, pia->b.direction_register);
		fs_write_uint8(fd, pia->b.output_register);
		fs_write_uint8(fd, pia->b.control_register);
	}
	// CPU state
	struct MC6809 *cpu = machine_get_cpu(0);
	switch (xroar_machine_config->cpu) {
	case CPU_MC6809: default:
		write_mc6809(fd, cpu);
		break;
	case CPU_HD6309:
		write_hd6309(fd, (struct HD6309 *)cpu);
		break;
	}
	// SAM
	write_chunk_header(fd, ID_SAM_REGISTERS, 2);
	fs_write_uint16(fd, sam_get_register());
	// Attached virtual disk filenames
	{
		for (unsigned drive = 0; drive < VDRIVE_MAX_DRIVES; drive++) {
			struct vdisk *disk = vdrive_disk_in_drive(drive);
			if (disk != NULL && disk->filename != NULL) {
				int length = strlen(disk->filename) + 1;
				write_chunk_header(fd, ID_VDISK_FILE, 1 + length);
				fs_write_uint8(fd, drive);
				fwrite(disk->filename, 1, length, fd);
			}
		}
	}
	// Finish up
	fclose(fd);
	return 0;
}

static int old_arch_mapping[4] = {
	MACHINE_DRAGON32,
	MACHINE_DRAGON64,
	MACHINE_TANO,
	MACHINE_COCOUS
};

static void old_set_registers(uint8_t *regs) {
	struct MC6809 *cpu = machine_get_cpu(0);
	cpu->reg_cc = regs[0];
	MC6809_REG_A(cpu) = regs[1];
	MC6809_REG_B(cpu) = regs[2];
	cpu->reg_dp = regs[3];
	cpu->reg_x = regs[4] << 8 | regs[5];
	cpu->reg_y = regs[6] << 8 | regs[7];
	cpu->reg_u = regs[8] << 8 | regs[9];
	cpu->reg_s = regs[10] << 8 | regs[11];
	cpu->reg_pc = regs[12] << 8 | regs[13];
	cpu->halt = 0;
	cpu->nmi = 0;
	cpu->firq = 0;
	cpu->irq = 0;
	cpu->state = MC6809_COMPAT_STATE_NORMAL;
	cpu->nmi_armed = 0;
}

static uint16_t *tfm_reg_ptr(struct HD6309 *hcpu, unsigned reg) {
	struct MC6809 *cpu = &hcpu->mc6809;
	switch (reg >> 4) {
	case 0:
		return &cpu->reg_d;
	case 1:
		return &cpu->reg_x;
	case 2:
		return &cpu->reg_y;
	case 3:
		return &cpu->reg_u;
	case 4:
		return &cpu->reg_s;
	default:
		break;
	}
	return NULL;
}

#define sex4(v) (((uint16_t)(v) & 0x07) - ((uint16_t)(v) & 0x08))

int read_snapshot(const char *filename) {
	FILE *fd;
	uint8_t buffer[17];
	int section, tmp;
	int version_major = 1, version_minor = 0;
	if (filename == NULL)
		return -1;
	if (!(fd = fopen(filename, "rb")))
		return -1;
	if (fread(buffer, 17, 1, fd) < 1) {
		LOG_WARN("Snapshot format not recognised.\n");
		fclose(fd);
		return -1;
	}
	if (strncmp((char *)buffer, "XRoar snapshot.\012\000", 17)) {
		// Very old-style snapshot.  Register dump always came first.
		// Also, it used to be written out as only taking 12 bytes.
		if (buffer[0] != ID_REGISTER_DUMP || buffer[1] != 0
				|| (buffer[2] != 12 && buffer[2] != 14)) {
			LOG_WARN("Snapshot format not recognised.\n");
			fclose(fd);
			return -1;
		}
	}
	// Default to Dragon 64 for old snapshots
	xroar_machine_config = machine_config_by_arch(ARCH_DRAGON64);
	machine_configure(xroar_machine_config);
	machine_reset(RESET_HARD);
	// If old snapshot, buffer contains register dump
	if (buffer[0] != 'X') {
		old_set_registers(buffer + 3);
	}
	while ((section = fs_read_uint8(fd)) >= 0) {
		int size = fs_read_uint16(fd);
		if (size == 0) size = 0x10000;
		switch (section) {
			case ID_ARCHITECTURE:
				// Deprecated: Machine architecture
				if (size < 1) break;
				tmp = fs_read_uint8(fd);
				tmp %= 4;
				xroar_machine_config->architecture = old_arch_mapping[tmp];
				machine_configure(xroar_machine_config);
				machine_reset(RESET_HARD);
				size--;
				break;
			case ID_KEYBOARD_MAP:
				// Deprecated: Keyboard map
				if (size < 1) break;
				tmp = fs_read_uint8(fd);
				xroar_set_keymap(tmp);
				size--;
				break;
			case ID_REGISTER_DUMP:
				// Deprecated
				if (size < 14) break;
				size -= fread(buffer, 1, 14, fd);
				old_set_registers(buffer);
				break;

			case ID_MC6809_STATE:
				{
					// MC6809 state
					if (size < 20) break;
					if (xroar_machine_config->cpu != CPU_MC6809) {
						LOG_WARN("CPU mismatch - skipping MC6809 chunk\n");
						break;
					}
					struct MC6809 *cpu = machine_get_cpu(0);
					cpu->reg_cc = fs_read_uint8(fd);
					MC6809_REG_A(cpu) = fs_read_uint8(fd);
					MC6809_REG_B(cpu) = fs_read_uint8(fd);
					cpu->reg_dp = fs_read_uint8(fd);
					cpu->reg_x = fs_read_uint16(fd);
					cpu->reg_y = fs_read_uint16(fd);
					cpu->reg_u = fs_read_uint16(fd);
					cpu->reg_s = fs_read_uint16(fd);
					cpu->reg_pc = fs_read_uint16(fd);
					cpu->halt = fs_read_uint8(fd);
					cpu->nmi = fs_read_uint8(fd);
					cpu->firq = fs_read_uint8(fd);
					cpu->irq = fs_read_uint8(fd);
					if (size == 21) {
						// Old style
						int wait_for_interrupt;
						int skip_register_push;
						wait_for_interrupt = fs_read_uint8(fd);
						skip_register_push = fs_read_uint8(fd);
						if (wait_for_interrupt && skip_register_push) {
							cpu->state = MC6809_COMPAT_STATE_CWAI;
						} else if (wait_for_interrupt) {
							cpu->state = MC6809_COMPAT_STATE_SYNC;
						} else {
							cpu->state = MC6809_COMPAT_STATE_NORMAL;
						}
						size--;
					} else {
						cpu->state = fs_read_uint8(fd);
					}
					cpu->nmi_armed = fs_read_uint8(fd);
					size -= 20;
					if (size > 0) {
						// Skip 'halted'
						(void)fs_read_uint8(fd);
						size--;
					}
				}
				break;

			case ID_HD6309_STATE:
				{
					// HD6309 state
					if (size < 27) break;
					if (xroar_machine_config->cpu != CPU_HD6309) {
						LOG_WARN("CPU mismatch - skipping HD6309 chunk\n");
						break;
					}
					struct MC6809 *cpu = machine_get_cpu(0);
					struct HD6309 *hcpu = (struct HD6309 *)cpu;
					cpu->reg_cc = fs_read_uint8(fd);
					MC6809_REG_A(cpu) = fs_read_uint8(fd);
					MC6809_REG_B(cpu) = fs_read_uint8(fd);
					cpu->reg_dp = fs_read_uint8(fd);
					cpu->reg_x = fs_read_uint16(fd);
					cpu->reg_y = fs_read_uint16(fd);
					cpu->reg_u = fs_read_uint16(fd);
					cpu->reg_s = fs_read_uint16(fd);
					cpu->reg_pc = fs_read_uint16(fd);
					cpu->halt = fs_read_uint8(fd);
					cpu->nmi = fs_read_uint8(fd);
					cpu->firq = fs_read_uint8(fd);
					cpu->irq = fs_read_uint8(fd);
					hcpu->state = fs_read_uint8(fd);
					cpu->nmi_armed = fs_read_uint8(fd);
					HD6309_REG_E(hcpu) = fs_read_uint8(fd);
					HD6309_REG_F(hcpu) = fs_read_uint8(fd);
					hcpu->reg_v = fs_read_uint16(fd);
					tmp = fs_read_uint8(fd);
					hcpu->reg_md = tmp;
					tmp = fs_read_uint8(fd);
					hcpu->tfm_src = tfm_reg_ptr(hcpu, tmp >> 4);
					hcpu->tfm_dest = tfm_reg_ptr(hcpu, tmp & 15);
					tmp = fs_read_uint8(fd);
					hcpu->tfm_src_mod = sex4(tmp >> 4);
					hcpu->tfm_dest_mod = sex4(tmp & 15);
					size -= 27;
				}
				break;

			case ID_MACHINECONFIG:
				// Machine running config
				if (size < 7) break;
				(void)fs_read_uint8(fd);  // requested_machine
				tmp = fs_read_uint8(fd);
				xroar_machine_config = machine_config_by_arch(tmp);
				tmp = fs_read_uint8(fd);  // was romset
				if (version_minor >= 7) {
					// old field not used any more, repurposed
					// in v1.7 to hold cpu type:
					xroar_machine_config->cpu = tmp;
				}
				xroar_machine_config->keymap = fs_read_uint8(fd);  // keymap
				xroar_machine_config->tv_standard = fs_read_uint8(fd);
				xroar_machine_config->ram = fs_read_uint8(fd);
				tmp = fs_read_uint8(fd);  // dos_type
				xroar_set_dos(tmp);
				size -= 7;
				if (size > 0) {
					xroar_machine_config->cross_colour_phase = fs_read_uint8(fd);
					size--;
				}
				machine_configure(xroar_machine_config);
				machine_reset(RESET_HARD);
				break;

			case ID_PIA_REGISTERS:
				for (int i = 0; i < 2; i++) {
					struct MC6821 *pia = machine_get_pia(i);
					if (size < 3) break;
					pia->a.direction_register = fs_read_uint8(fd);
					pia->a.output_register = fs_read_uint8(fd);
					pia->a.control_register = fs_read_uint8(fd);
					size -= 3;
					if (size < 3) break;
					pia->b.direction_register = fs_read_uint8(fd);
					pia->b.output_register = fs_read_uint8(fd);
					pia->b.control_register = fs_read_uint8(fd);
					size -= 3;
					mc6821_update_state(pia);
				}
				break;

			case ID_RAM_PAGE0:
				if (size <= (int)sizeof(machine_ram)) {
					size -= fread(machine_ram, 1, size, fd);
				} else {
					size -= fread(machine_ram, 1, sizeof(machine_ram), fd);
				}
				break;
			case ID_RAM_PAGE1:
				if (size <= (int)(sizeof(machine_ram) - 0x8000)) {
					size -= fread(machine_ram + 0x8000, 1, size, fd);
				} else {
					size -= fread(machine_ram + 0x8000, 1, sizeof(machine_ram) - 0x8000, fd);
				}
				break;
			case ID_SAM_REGISTERS:
				// SAM
				if (size < 2) break;
				tmp = fs_read_uint16(fd);
				size -= 2;
				sam_set_register(tmp);
				break;

			case ID_SNAPVERSION:
				// Snapshot version - abort if snapshot
				// contains stuff we don't understand
				if (size < 3) break;
				version_major = fs_read_uint8(fd);
				version_minor = fs_read_uint16(fd);
				size -= 3;
				if (version_major != SNAPSHOT_VERSION_MAJOR
				    || version_minor > SNAPSHOT_VERSION_MINOR) {
					LOG_WARN("Snapshot version %d.%d not supported.\n", version_major, version_minor);
					fclose(fd);
					return -1;
				}
				break;

			case ID_VDISK_FILE:
				// Attached virtual disk filenames
				{
					int drive;
					size--;
					drive = fs_read_uint8(fd);
					vdrive_eject_disk(drive);
					if (size > 0) {
						char *name = g_try_malloc(size);
						if (name != NULL) {
							size -= fread(name, 1, size, fd);
							vdrive_insert_disk(drive, vdisk_load(name));
						}
					}
				}
				break;
			default:
				// Unknown chunk
				LOG_WARN("Unknown chunk in snaphot.\n");
				break;
		}
		if (size > 0) {
			LOG_WARN("Skipping extra bytes in snapshot chunk id=%d.\n", (int)section);
			for (; size; size--)
				(void)fs_read_uint8(fd);
		}
	}
	fclose(fd);
	return 0;
}
