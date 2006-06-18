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

/* TODO: Currently possibly (and likely) to save a snapshot with an interrupt
 * pending, so need to save CPU interrupt flags */

/* Reorganised to write files in 'chunks', each with an identifying byte and
 * a 16-bit length.  This should mean no more changes are required that break
 * the format (if I think of a better way of doing something I just give it a
 * different id) */

#define ID_REGISTER_DUMP (0)
#define ID_RAM_PAGE0     (1)
#define ID_PIA_REGISTERS (2)
#define ID_SAM_REGISTERS (3)

int write_snapshot(char *filename) {
	int fd;
	uint8_t buffer[14];
	if ((fd = fs_open(filename, FS_WRITE)) == -1)
		return -1;
	fs_write(fd, "XRoar snapshot.\012\000", 17);
	m6809_get_registers(buffer);
	fs_write_byte(fd, ID_REGISTER_DUMP); fs_write_word16(fd, 14);
	fs_write(fd, buffer, 14);
	fs_write_byte(fd, ID_RAM_PAGE0); fs_write_word16(fd, sizeof(ram0));
	fs_write(fd, ram0, sizeof(ram0));
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
	/* SAM */
	fs_write_byte(fd, ID_SAM_REGISTERS); fs_write_word16(fd, 2);
	fs_write_word16(fd, sam_register);
	fs_close(fd);
	return 0;
}

int read_snapshot(char *filename) {
	int fd;
	uint8_t buffer[17];
	uint8_t section, tmp8;
	uint16_t size, tmp16;
	if (filename == NULL)
		return -1;
	if ((fd = fs_open(filename, FS_READ)) == -1)
		return -1;
	machine_reset(RESET_HARD);
	fs_read(fd, buffer, 17);
	if (strncmp((char *)buffer, "XRoar snapshot.\012\000", 17)) {
		/* Old-style snapshot.  Register dump always came first.
		 * Also, it used to be written out as only taking 12 bytes. */
		if (buffer[0] != ID_REGISTER_DUMP || buffer[1] != 0
				|| (buffer[2] != 12 && buffer[2] != 14)) {
			fs_close(fd);
			return -1;
		}
		m6809_set_registers(buffer + 3);
	}
	while (fs_read_byte(fd, &section) > 0) {
		fs_read_word16(fd, &size);
		switch (section) {
			case ID_REGISTER_DUMP:
				fs_read(fd, buffer, 14);
				m6809_set_registers(buffer);
				break;
			case ID_RAM_PAGE0:
				fs_read(fd, ram0, sizeof(ram0));
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
			case ID_SAM_REGISTERS:
				/* SAM */
				fs_read_word16(fd, &tmp16);
				sam_register = tmp16;
				sam_update_from_register();
				size -= 2;
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
