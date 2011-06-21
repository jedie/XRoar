/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2011  Ciaran Anscomb
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Sources:
 *     WD279x:
 *         http://www.swtpc.com/mholley/DC_5/TMS279X_DataSheet.pdf
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "crc16.h"
#include "events.h"
#include "logging.h"
#include "machine.h"
#include "vdrive.h"
#include "wd279x.h"
#include "xroar.h"

#define STATUS_NOT_READY     (1<<7)
#define STATUS_WRITE_PROTECT (1<<6)
#define STATUS_HEAD_LOADED   (1<<5)
#define STATUS_RECORD_TYPE   (1<<5)
#define STATUS_SEEK_ERROR    (1<<4)
#define STATUS_RNF           (1<<4)
#define STATUS_CRC_ERROR     (1<<3)
#define STATUS_TRACK_0       (1<<2)
#define STATUS_LOST_DATA     (1<<2)
#define STATUS_INDEX_PULSE   (1<<1)
#define STATUS_DRQ           (1<<1)
#define STATUS_BUSY          (1<<0)

#define W_MILLISEC(ms) ((OSCILLATOR_RATE/1000)*(ms))
#define W_MICROSEC(us) ((OSCILLATOR_RATE*(us))/1000000)

#define W_BYTE_TIME (OSCILLATOR_RATE / 31250)

#define SET_DRQ do { \
		status_register |= STATUS_DRQ; \
		if (wd279x_set_drq_handler) \
			wd279x_set_drq_handler(); \
	} while (0)
#define RESET_DRQ do { \
		status_register &= ~(STATUS_DRQ); \
		if (wd279x_reset_drq_handler) \
			wd279x_reset_drq_handler(); \
	} while (0)
#define SET_INTRQ do { \
		if (wd279x_set_intrq_handler) \
			wd279x_set_intrq_handler(); \
	} while (0)
#define RESET_INTRQ do { \
		if (wd279x_reset_intrq_handler) \
			wd279x_reset_intrq_handler(); \
	} while (0)

#define NEXT_STATE(f,t) do { \
		state = f; \
		state_event.at_cycle = current_cycle + t; \
		event_queue(&MACHINE_EVENT_LIST, &state_event); \
	} while (0)
#define GOTO_STATE(f) state = f; continue

#define SINGLE_DENSITY (0)
#define DOUBLE_DENSITY (1)

#define IS_DOUBLE_DENSITY (density == DOUBLE_DENSITY)
#define IS_SINGLE_DENSITY (density == SINGLE_DENSITY)

#define SET_DIRECTION do { direction = 1; vdrive_set_direction(1); } while (0)
#define RESET_DIRECTION do { \
		direction = -1; vdrive_set_direction(-1); \
	} while (0)
#define SET_SIDE(s) do { \
		side = (s) ? 1 : 0; \
		if (HAS_SSO) \
			vdrive_set_side(side); \
	} while (0)

/* From enum WD279X_type */
unsigned int wd279x_type;

#define HAS_SSO (wd279x_type == WD2795 || wd279x_type == WD2797)
#define HAS_LENGTH_FLAG (wd279x_type == WD2795 || wd279x_type == WD2797)
#define INVERTED_DATA (wd279x_type == WD2791 || wd279x_type == WD2795)

/* Various signal handlers set by DOS cart reset code: */
void (*wd279x_set_drq_handler)(void);
void (*wd279x_reset_drq_handler)(void);
void (*wd279x_set_intrq_handler)(void);
void (*wd279x_reset_intrq_handler)(void);

/* FDC states: */
enum wd279x_state {
	accept_command,
	type_1_state_1,
	type_1_state_2,
	type_1_state_3,
	verify_track_state_1,
	verify_track_state_2,
	type_2_state_1,
	type_2_state_2,
	read_sector_state_1,
	read_sector_state_2,
	read_sector_state_3,
	write_sector_state_1,
	write_sector_state_2,
	write_sector_state_3,
	write_sector_state_4,
	write_sector_state_5,
	write_sector_state_6,
	type_3_state_1,
	read_address_state_1,
	read_address_state_2,
	read_address_state_3,
	write_track_state_1,
	write_track_state_2,
	write_track_state_2b,
	write_track_state_3
};

static void state_machine(void);
static event_t state_event;
static enum wd279x_state state;

/* WD279X registers */
static unsigned int status_register;
static unsigned int track_register;
static unsigned int sector_register;
static unsigned int data_register;

/* WD279X internal state */
static unsigned int command_register;
static unsigned int is_step_cmd;
static int direction;
static unsigned int side;
static unsigned int step_delay;
static unsigned int index_holes_count;
static unsigned int bytes_left;
static unsigned int dam;
static unsigned int track_register_tmp;

static unsigned int stepping_rate[4] = { 6, 12, 20, 30 };
static unsigned int sector_size[2][4] = {
	{ 256, 512, 1024, 128 },
	{ 128, 256, 512, 1024 }
};

static unsigned int density;

void wd279x_init(void) {
	wd279x_type = WD2797;
	wd279x_set_drq_handler = NULL;
	wd279x_reset_drq_handler = NULL;
	wd279x_set_intrq_handler = NULL;
	wd279x_reset_intrq_handler = NULL;
	event_init(&state_event);
	state_event.dispatch = state_machine;
}

void wd279x_reset(void) {
	event_dequeue(&state_event);
	status_register = 0;
	track_register = 0;
	sector_register = 0;
	data_register = 0;
	command_register = 0;
	RESET_DIRECTION;
	SET_SIDE(0);
}

void wd279x_set_density(unsigned int d) {
	/* DDEN# is active-low */
	density = d ? SINGLE_DENSITY : DOUBLE_DENSITY;
	vdrive_set_density(d ? VDISK_SINGLE_DENSITY : VDISK_DOUBLE_DENSITY);
}

void wd279x_track_register_write(unsigned int octet) {
	if (INVERTED_DATA)
		octet = ~octet;
	track_register = octet & 0xff;
}

unsigned int wd279x_track_register_read(void) {
	if (INVERTED_DATA)
		return ~track_register & 0xff;
	return track_register & 0xff;
}

void wd279x_sector_register_write(unsigned int octet) {
	if (INVERTED_DATA)
		octet = ~octet;
	sector_register = octet & 0xff;
}

unsigned int wd279x_sector_register_read(void) {
	if (INVERTED_DATA)
		return ~sector_register & 0xff;
	return sector_register & 0xff;
}

void wd279x_data_register_write(unsigned int octet) {
	RESET_DRQ;
	if (INVERTED_DATA)
		octet = ~octet;
	data_register = octet & 0xff;
}

unsigned int wd279x_data_register_read(void) {
	RESET_DRQ;
	if (INVERTED_DATA)
		return ~data_register & 0xff;
	return data_register & 0xff;
}

unsigned int wd279x_status_read(void) {
	RESET_INTRQ;
	if (vdrive_ready)
		status_register &= ~STATUS_NOT_READY;
	else
		status_register |= STATUS_NOT_READY;
	if ((command_register & 0xf0) == 0xd0 || (command_register & 0x80) == 0x00) {
		if (vdrive_tr00)
			status_register |= STATUS_TRACK_0;
		else
			status_register &= ~STATUS_TRACK_0;
		if (vdrive_index_pulse)
			status_register |= STATUS_INDEX_PULSE;
		else
			status_register &= ~STATUS_INDEX_PULSE;
	}
	if (INVERTED_DATA)
		return ~status_register & 0xff;
	return status_register & 0xff;
}

void wd279x_command_write(unsigned int cmd) {
	RESET_INTRQ;
	if (INVERTED_DATA)
		command_register = ~cmd & 0xff;
	else
		command_register = cmd & 0xff;
	/* FORCE INTERRUPT */
	if ((command_register & 0xf0) == 0xd0) {
		LOG_DEBUG(4,"WD279X: CMD: Force interrupt (%01x)\n",command_register&0x0f);
		if ((command_register & 0x0f) == 0) {
			event_dequeue(&state_event);
			status_register &= ~(STATUS_BUSY);
			return;
		}
		if (command_register & 0x08) {
			event_dequeue(&state_event);
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;
		}
		return;
	}
	/* Ignore any other command if busy */
	if (status_register & STATUS_BUSY) {
		LOG_DEBUG(4,"WD279X: Command received while busy!\n");
		return;
	}
	state = accept_command;
	state_machine();
}

/* One big state machine.  This is called from an event dispatch and from the
 * write command function. */

static void state_machine(void) {
	uint8_t *idam;
	unsigned int data;
	int i;
	for (;;) {
		switch (state) {

		case accept_command:
			/* 0xxxxxxx = RESTORE / SEEK / STEP / STEP-IN / STEP-OUT */
			if ((command_register & 0x80) == 0x00) {
				status_register |= STATUS_BUSY;
				status_register &= ~(STATUS_CRC_ERROR|STATUS_SEEK_ERROR);
				RESET_DRQ;
				step_delay = stepping_rate[command_register & 3];
				is_step_cmd = 0;
				if ((command_register & 0xe0) == 0x20) {
					LOG_DEBUG(4, "WD279X: CMD: Step\n");
					is_step_cmd = 1;
				} else if ((command_register & 0xe0) == 0x40) {
					LOG_DEBUG(4, "WD279X: CMD: Step-in\n");
					is_step_cmd = 1;
					SET_DIRECTION;
				} else if ((command_register & 0xe0) == 0x60) {
					LOG_DEBUG(4, "WD279X: CMD: Step-out\n");
					is_step_cmd = 1;
					RESET_DIRECTION;
				}
				if (is_step_cmd) {
					if (command_register & 0x10) {
						GOTO_STATE(type_1_state_2);
					}
					GOTO_STATE(type_1_state_3);
				}
				if ((command_register & 0xf0) == 0x00) {
					track_register = 0xff;
					data_register = 0x00;
					LOG_DEBUG(4, "WD279X: CMD: Restore\n");
				} else {
					LOG_DEBUG(4, "WD279X: CMD: Seek (TR=%d)\n", data_register);
				}
				GOTO_STATE(type_1_state_1);
			}

			/* 10xxxxxx = READ/WRITE SECTOR */
			if ((command_register & 0xc0) == 0x80) {
				if ((command_register & 0xe0) == 0x80) {
					LOG_DEBUG(4, "WD279X: CMD: Read sector (Tr %d, Side %d, Sector %d)\n", track_register, side, sector_register);
				} else {
					LOG_DEBUG(4, "WD279X: CMD: Write sector\n");
				}
				status_register |= STATUS_BUSY;
				status_register &= ~(STATUS_LOST_DATA|STATUS_RNF|(1<<5)|(1<<6));
				RESET_DRQ;
				if (!vdrive_ready) {
					status_register &= ~(STATUS_BUSY);
					SET_INTRQ;
					return;
				}
				if (HAS_SSO)
					SET_SIDE(command_register & 0x02);  /* 'U' */
				else
					SET_SIDE(command_register & 0x08);  /* 'S' */
				if (command_register & 0x04) {  /* 'E' set */
					NEXT_STATE(type_2_state_1, W_MILLISEC(30));
					return;
				}
				GOTO_STATE(type_2_state_1);
			}

			/* 11000xx0 = READ ADDRESS */
			/* 11100xx0 = READ TRACK */
			/* 11110xx0 = WRITE TRACK */
			if (((command_register & 0xf9) == 0xc0)
					|| ((command_register & 0xf9) == 0xe0)
					|| ((command_register & 0xf9) == 0xf0)) {
				status_register |= STATUS_BUSY;
				status_register &= ~(STATUS_LOST_DATA|(1<<4)|(1<<5));
				if ((command_register & 0xf0) == 0xf0)
					RESET_DRQ;
				if (!vdrive_ready) {
					status_register &= ~(STATUS_BUSY);
					SET_INTRQ;
					return;
				}
				if (HAS_SSO)
					SET_SIDE(command_register & 0x02);  /* 'U' */
				else
					SET_SIDE(command_register & 0x08);  /* 'S' */
				if (command_register & 0x04) {  /* 'E' set */
					NEXT_STATE(type_3_state_1, W_MILLISEC(30));
					return;
				}
				GOTO_STATE(type_3_state_1);
			}
			LOG_WARN("WD279X: CMD: Unknown command %02x\n", command_register);
			return;


		case type_1_state_1:
			LOG_DEBUG(5, " type_1_state_1()\n");
			if (data_register == track_register) {
				GOTO_STATE(verify_track_state_1);
			}
			if (data_register > track_register)
				SET_DIRECTION;
			else
				RESET_DIRECTION;
			/* GOTO_STATE(type_1_state_2); */
			/* fall through */


		case type_1_state_2:
			LOG_DEBUG(5, " type_1_state_2()\n");
			track_register += direction;
			/* GOTO_STATE(type_1_state_3); */
			/* fall through */


		case type_1_state_3:
			LOG_DEBUG(5, " type_1_state_3()\n");
			if (vdrive_tr00 && direction == -1) {
				LOG_DEBUG(4,"WD279X: TR00!\n");
				track_register = 0;
				GOTO_STATE(verify_track_state_1);
			}
			vdrive_step();
			if (is_step_cmd) {
				NEXT_STATE(verify_track_state_1, W_MILLISEC(step_delay));
				return;
			}
			NEXT_STATE(type_1_state_1, W_MILLISEC(step_delay));
			return;


		case verify_track_state_1:
			LOG_DEBUG(5, " verify_track_state_1()\n");
			if (!(command_register & 0x04)) {
				status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			index_holes_count = 0;
			NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
			return;


		case verify_track_state_2:
			LOG_DEBUG(5, " verify_track_state_2()\n");
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				index_holes_count++;
				if (index_holes_count >= 5) {
					LOG_DEBUG(5, "index_holes_count >= 5: seek error\n");
					status_register &= ~(STATUS_BUSY);
					status_register |= STATUS_SEEK_ERROR;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				LOG_DEBUG(5, "null IDAM: -> verify_track_state_2\n");
				NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
				return;
			}
			crc16_reset();
			if (IS_DOUBLE_DENSITY) {
				crc16_byte(0xa1);
				crc16_byte(0xa1);
				crc16_byte(0xa1);
			}
			(void)vdrive_read();  /* Include IDAM in CRC */
			if (track_register != vdrive_read()) {
				LOG_DEBUG(5, "track_register != idam[1]: -> verify_track_state_2\n");
				NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
				return;
			}
			/* Include rest of ID field - should result in computed CRC = 0 */
			for (i = 0; i < 5; i++)
				(void)vdrive_read();
			if (crc16_value() != 0) {
				LOG_DEBUG(3, "Verify track %d CRC16 error: $%04x != 0\n", track_register, crc16_value());
				status_register |= STATUS_CRC_ERROR;
				NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
				return;
			}
			LOG_DEBUG(5, "finished.\n");
			status_register &= ~(STATUS_CRC_ERROR|STATUS_BUSY);
			SET_INTRQ;
			return;


		case type_2_state_1:
			LOG_DEBUG(5, " type_2_state_1()\n");
			if ((command_register & 0x20) && vdrive_write_protect) {
				status_register &= ~(STATUS_BUSY);
				status_register |= STATUS_WRITE_PROTECT;
				SET_INTRQ;
				return;
			}
			index_holes_count = 0;
			NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
			return;


		case type_2_state_2:
			LOG_DEBUG(5, " type_2_state_2()\n");
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				index_holes_count++;
				if (index_holes_count >= 5) {
					status_register &= ~(STATUS_BUSY);
					status_register |= STATUS_RNF;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
				return;
			}
			crc16_reset();
			if (IS_DOUBLE_DENSITY) {
				crc16_byte(0xa1);
				crc16_byte(0xa1);
				crc16_byte(0xa1);
			}
			(void)vdrive_read();  /* Include IDAM in CRC */
			if (track_register != vdrive_read()) {
				NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
				return;
			}
			if (side != vdrive_read()) {
				/* No error if no SSO or 'C' not set */
				if (HAS_SSO || command_register & 0x02) {
					NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
					return;
				}
			}
			if (sector_register != vdrive_read()) {
				NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
				return;
			}
			i = vdrive_read();
			if (HAS_LENGTH_FLAG)
				bytes_left = sector_size[(command_register & 0x08)?1:0][i&3];
			else
				bytes_left = sector_size[1][i&3];
			/* Including CRC bytes should result in computed CRC = 0 */
			(void)vdrive_read();
			(void)vdrive_read();
			if (crc16_value() != 0) {
				status_register |= STATUS_CRC_ERROR;
				LOG_DEBUG(3, "Type 2 tr %d se %d CRC16 error: $%04x != 0\n", track_register, sector_register, crc16_value());
				NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
				return;
			}

			if ((command_register & 0x20) == 0) {
				unsigned int bytes_to_scan, j, tmp;
				if (IS_SINGLE_DENSITY)
					bytes_to_scan = 30;
				else
					bytes_to_scan = 43;
				j = 0;
				dam = 0;
				do {
					crc16_reset();
					if (IS_DOUBLE_DENSITY) {
						crc16_byte(0xa1);
						crc16_byte(0xa1);
						crc16_byte(0xa1);
					}
					tmp = vdrive_read();
					if (tmp == 0xfb || tmp == 0xf8)
						dam = tmp;
					j++;
				} while (j < bytes_to_scan && dam == 0);
				if (dam == 0) {
					NEXT_STATE(type_2_state_2, vdrive_time_to_next_byte());
					return;
				}
				NEXT_STATE(read_sector_state_1, vdrive_time_to_next_byte());
				return;
			}
			vdrive_skip();
			vdrive_skip();
			NEXT_STATE(write_sector_state_1, vdrive_time_to_next_byte());
			return;


		case read_sector_state_1:
			LOG_DEBUG(5, " read_sector_state_1()\n");
			LOG_DEBUG(4,"Reading %d-byte sector (Tr %d, Se %d) from head_pos=%04x\n", bytes_left, track_register, sector_register, vdrive_head_pos());
			status_register |= ((~dam & 1) << 5);
			data_register = vdrive_read();
			bytes_left--;
			SET_DRQ;
			NEXT_STATE(read_sector_state_2, vdrive_time_to_next_byte());
			return;


		case read_sector_state_2:
			LOG_DEBUG(5, " read_sector_state_2()\n");
			if (status_register & STATUS_DRQ) {
				status_register |= STATUS_LOST_DATA;
				//RESET_DRQ;  // XXX
			}
			if (bytes_left > 0) {
				data_register = vdrive_read();
				bytes_left--;
				SET_DRQ;
				NEXT_STATE(read_sector_state_2, vdrive_time_to_next_byte());
				return;
			}
			/* Including CRC bytes should result in computed CRC = 0 */
			(void)vdrive_read();
			(void)vdrive_read();
			NEXT_STATE(read_sector_state_3, vdrive_time_to_next_byte());
			return;


		case read_sector_state_3:
			LOG_DEBUG(5, " read_sector_state_3()\n");
			if (crc16_value() != 0) {
				LOG_DEBUG(3, "Read sector data tr %d se %d CRC16 error: $%04x != 0\n", track_register, sector_register, crc16_value());
				status_register |= STATUS_CRC_ERROR;
			}
			/* TODO: M == 1 */
			if (command_register & 0x10) {
				LOG_DEBUG(2, "WD279X: TODO: multi-sector read will fail.\n");
			}
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case write_sector_state_1:
			LOG_DEBUG(5, " write_sector_state_1()\n");
			SET_DRQ;
			for (i = 0; i < 8; i++)
				vdrive_skip();
			NEXT_STATE(write_sector_state_2, vdrive_time_to_next_byte());
			return;


		case write_sector_state_2:
			LOG_DEBUG(5, " write_sector_state_2()\n");
			if (status_register & STATUS_DRQ) {
				status_register &= ~(STATUS_BUSY);
				RESET_DRQ;  // XXX
				status_register |= STATUS_LOST_DATA;
				SET_INTRQ;
				return;
			}
			vdrive_skip();
			NEXT_STATE(write_sector_state_3, vdrive_time_to_next_byte());
			return;


		case write_sector_state_3:
			LOG_DEBUG(5, " write_sector_state_3()\n");
			if (IS_DOUBLE_DENSITY) {
				for (i = 0; i < 11; i++)
					vdrive_skip();
				for (i = 0; i < 12; i++)
					vdrive_write(0);
				NEXT_STATE(write_sector_state_4, vdrive_time_to_next_byte());
				return;
			}
			for (i = 0; i < 6; i++)
				vdrive_write(0);
			NEXT_STATE(write_sector_state_4, vdrive_time_to_next_byte());
			return;


		case write_sector_state_4:
			LOG_DEBUG(5, " write_sector_state_4()\n");
			crc16_reset();
			if (IS_DOUBLE_DENSITY) {
				crc16_byte(0xa1);
				crc16_byte(0xa1);
				crc16_byte(0xa1);
			}
			if (command_register & 1)
				vdrive_write(0xf8);
			else
				vdrive_write(0xfb);
			NEXT_STATE(write_sector_state_5, vdrive_time_to_next_byte());
			return;


		case write_sector_state_5:
			data = data_register;
			LOG_DEBUG(5, " write_sector_state_5()\n");
			if (status_register & STATUS_DRQ) {
				data = 0;
				status_register |= STATUS_LOST_DATA;
				RESET_DRQ;  // XXX
			}
			vdrive_write(data);
			bytes_left--;
			if (bytes_left > 0) {
				SET_DRQ;
				NEXT_STATE(write_sector_state_5, vdrive_time_to_next_byte());
				return;
			}
			VDRIVE_WRITE_CRC16;
			NEXT_STATE(write_sector_state_6, vdrive_time_to_next_byte() + W_MICROSEC(20));
			return;


		case write_sector_state_6:
			LOG_DEBUG(5, " write_sector_state_6()\n");
			vdrive_write(0xfe);
			/* TODO: M = 1 */
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case type_3_state_1:
			LOG_DEBUG(5, " type_3_state_1()\n");
			switch (command_register & 0xf0) {
				case 0xc0:
					index_holes_count = 0;
					NEXT_STATE(read_address_state_1, vdrive_time_to_next_idam());
					return;
				case 0xe0:
					LOG_WARN("WD279X: CMD: Read track not implemented\n");
					SET_INTRQ;
					break;
				case 0xf0:
					LOG_DEBUG(4, "WD279X: CMD: Write track (Tr %d)\n", track_register);
					GOTO_STATE(write_track_state_1);
				default:
					LOG_WARN("WD279X: CMD: Unknown command %02x\n", command_register);
					break;
			}
			return;


		case read_address_state_1:
			LOG_DEBUG(5, " read_address_state_1()\n");
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				index_holes_count++;
				if (index_holes_count >= 6) {
					status_register &= ~(STATUS_BUSY);
					status_register |= STATUS_RNF;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				NEXT_STATE(read_address_state_1, vdrive_time_to_next_idam());
				return;
			}
			crc16_reset();
			if (IS_DOUBLE_DENSITY) {
				crc16_byte(0xa1);
				crc16_byte(0xa1);
				crc16_byte(0xa1);
			}
			(void)vdrive_read();
			NEXT_STATE(read_address_state_2, vdrive_time_to_next_byte());
			return;


		case read_address_state_2:
			bytes_left = 5;
			data_register = vdrive_read();
			/* At end of command, this is transferred to the sector register: */
			track_register_tmp = data_register;
			SET_DRQ;
			NEXT_STATE(read_address_state_3, vdrive_time_to_next_byte());
			return;


		case read_address_state_3:
			/* Lost data not mentioned in data sheet, so not checking
			   for now */
			if (bytes_left > 0) {
				data_register = vdrive_read();
				bytes_left--;
				SET_DRQ;
				NEXT_STATE(read_address_state_3, vdrive_time_to_next_byte());
				return;
			}
			sector_register = track_register_tmp;
			if (crc16_value() != 0) {
				status_register |= STATUS_CRC_ERROR;
			}
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case write_track_state_1:
			LOG_DEBUG(5, " write_track_state_1()\n");
			if (vdrive_write_protect) {
				status_register &= ~(STATUS_BUSY);
				status_register |= STATUS_WRITE_PROTECT;
				SET_INTRQ;
				return;
			}
			SET_DRQ;
			/* Data sheet says 3 byte times, but CoCo NitrOS9 fails unless I set
			 * this delay higher. */
			NEXT_STATE(write_track_state_2, 6 * W_BYTE_TIME);
			return;


		case write_track_state_2:
			LOG_DEBUG(5, " write_track_state_2()\n");
			if (status_register & STATUS_DRQ) {
				RESET_DRQ;  // XXX
				status_register |= STATUS_LOST_DATA;
				status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			NEXT_STATE(write_track_state_2b, vdrive_time_to_next_idam());
			return;


		case write_track_state_2b:
			LOG_DEBUG(5, " write_track_state_2b()\n");
			if (!vdrive_new_index_pulse()) {
				LOG_DEBUG(4,"Waiting for index pulse, head_pos=%04x\n", vdrive_head_pos());
				NEXT_STATE(write_track_state_2b, vdrive_time_to_next_idam());
				return;
			}
			LOG_DEBUG(4,"Writing track from head_pos=%04x\n", vdrive_head_pos());
			/* GOTO_STATE(write_track_state_3); */
			/* fall through */


		case write_track_state_3:
			data = data_register;
			LOG_DEBUG(5, " write_track_state_3()\n");
			if (vdrive_new_index_pulse()) {
				LOG_DEBUG(4,"Finished writing track at head_pos=%04x\n", vdrive_head_pos());
				RESET_DRQ;  // XXX
				status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			if (status_register & STATUS_DRQ) {
				data = 0;
				status_register |= STATUS_LOST_DATA;
			}
			SET_DRQ;
			if (IS_SINGLE_DENSITY) {
				/* Single density */
				if (data == 0xf5 || data == 0xf6) {
					LOG_DEBUG(4, "Illegal value in single-density track write: %02x\n", data);
				}
				if (data == 0xf7) {
					VDRIVE_WRITE_CRC16;
					NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
					return;
				}
				if (data >= 0xf8 && data <= 0xfb) {
					crc16_reset();
					vdrive_write(data);
					NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
					return;
				}
				if (data == 0xfe) {
					LOG_DEBUG(4,"IDAM at head_pos=%04x\n", vdrive_head_pos());
					crc16_reset();
					vdrive_write_idam();
					NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
					return;
				}
				vdrive_write(data);
				NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
				return;
			}
			/* Double density */
			if (data == 0xf7) {
				VDRIVE_WRITE_CRC16;
				NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xfe) {
				LOG_DEBUG(4,"IDAM at head_pos=%04x\n", vdrive_head_pos());
				vdrive_write_idam();
				NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xf5) {
				crc16_reset();
				crc16_byte(0xa1);
				crc16_byte(0xa1);
				vdrive_write(0xa1);
				NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xf6) {
				data = 0xc2;
			}
			vdrive_write(data);
			NEXT_STATE(write_track_state_3, vdrive_time_to_next_byte());
			return;

		}
	}
}
