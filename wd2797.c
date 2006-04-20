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

/* Sources:
 *     WD279x:
 *         http://www.swtpc.com/mholley/DC_5/TMS279X_DataSheet.pdf
 *     DragonDOS cartridge detail:
 *         http://www.dragon-archive.co.uk/
 *     CoCo DOS cartridge detail:
 *         http://www.coco3.com/unravalled/disk-basic-unravelled.pdf
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "crc16.h"
#include "events.h"
#include "logging.h"
#include "m6809.h"
#include "pia.h"
#include "vdrive.h"
#include "wd2797.h"
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
		if (IS_COCO) { \
			halt = 0; \
		} else { \
			PIA_SET_P1CB1; \
		} \
	} while (0)
#define RESET_DRQ do { \
		status_register &= ~(STATUS_DRQ); \
		if (IS_COCO) { \
			if (halt_enable) { \
				halt = 1; \
			} \
		} else { \
			PIA_RESET_P1CB1; \
		} \
	} while (0)
#define SET_INTRQ do { \
		if (IS_COCO) { \
			halt_enable = halt = 0; \
		} \
		if (ic1_nmi_enable) \
			nmi = 1; \
	} while (0)
#define RESET_INTRQ

#define NEXT_STATE(f,t) do { \
		state_event->dispatch = f; \
		state_event->at_cycle = current_cycle + t; \
		event_queue(state_event); \
	} while (0)

#define IS_DOUBLE_DENSITY (ic1_density == 0)
#define IS_SINGLE_DENSITY (!IS_DOUBLE_DENSITY)

#define SET_DIRECTION do { direction = 1; vdrive_set_direction(1); } while (0)
#define RESET_DIRECTION do { \
		direction = -1; vdrive_set_direction(-1); \
	} while (0)
#define SET_SIDE(s) do { side = (s) ? 1 : 0; vdrive_set_side(side); } while (0)

/* Functions used in state machines: */
static void type_1_state_1(void);
static void type_1_state_2(void);
static void type_1_state_3(void);
static void verify_track_state_1(void);
static void verify_track_state_2(void);
static void type_2_state_1(void);
static void type_2_state_2(void);
static void read_sector_state_1(void);
static void read_sector_state_2(void);
static void read_sector_state_3(void);
static void write_sector_state_1(void);
static void write_sector_state_2(void);
static void write_sector_state_3(void);
static void write_sector_state_4(void);
static void write_sector_state_5(void);
static void write_sector_state_6(void);
static void write_track_state_1(void);
static void write_track_state_2(void);
static void write_track_state_3(void);

static event_t *state_event;

/* WD2797 registers */
static unsigned int status_register;
static unsigned int track_register;
static unsigned int sector_register;
static unsigned int data_register;

/* WD2797 internal state */
static unsigned int cmd_copy;
static unsigned int is_step_cmd;
static int direction;
static unsigned int side;
static unsigned int step_delay;
static unsigned int index_holes_count;
static unsigned int bytes_left;
static unsigned int dam;

static unsigned int stepping_rate[4] = { 6, 12, 20, 30 };
static unsigned int sector_size[2][4] = {
	{ 256, 512, 1024, 128 },
	{ 128, 256, 512, 1024 }
};

/* Not really part of WD2797, but also part of the DragonDOS cart: */
static unsigned int ic1_drive_select;
static unsigned int ic1_motor_enable;
static unsigned int ic1_precomp_enable;
static unsigned int ic1_density;
static unsigned int ic1_nmi_enable;
static unsigned int halt_enable;

void wd2797_init(void) {
	state_event = event_new();
}

void wd2797_reset(void) {
	event_dequeue(state_event);
	status_register = 0;
	track_register = 0;
	sector_register = 0;
	data_register = 0;
	if (IS_COCO)
		wd2797_ff40_write(0);
	else
		wd2797_ff48_write(0);
	RESET_DIRECTION;
	SET_SIDE(0);
}

/* Not strictly WD2797 accesses here, this is DOS cartridge circuitry */
void wd2797_ff48_write(unsigned int octet) {
	LOG_DEBUG(4, "WD2797: Write to FF48: ");
	if ((octet & 0x03) != ic1_drive_select)
		LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
	if ((octet & 0x04) != ic1_motor_enable)
		LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x04)?"ON":"OFF");
	if ((octet & 0x08) != ic1_density)
		LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x08)?"SINGLE":"DOUBLE");
	if ((octet & 0x10) != ic1_precomp_enable)
		LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
	if ((octet & 0x20) != ic1_nmi_enable)
		LOG_DEBUG(4, "NMI %s, ", (octet & 0x20)?"ENABLED":"DISABLED");
	LOG_DEBUG(4, "\n");
	ic1_drive_select = octet & 0x03;
	vdrive_set_drive(ic1_drive_select);
	ic1_motor_enable = octet & 0x04;
	ic1_density = octet & 0x08;
	vdrive_set_density(ic1_density);
	ic1_precomp_enable = octet & 0x10;
	ic1_nmi_enable = octet & 0x20;
}

/* Coco version of above - slightly different... */
void wd2797_ff40_write(unsigned int octet) {
	unsigned int new_drive_select = 0;
	octet ^= 0x20;
	if (octet & 0x01) {
		new_drive_select = 0;
	} else if (octet & 0x02) {
		new_drive_select = 1;
	} else if (octet & 0x04) {
		new_drive_select = 2;
	} else if (octet & 0x40) {
		new_drive_select = 3;
	}
	//LOG_DEBUG(4, "WD2797: Write to FF40: ");
	if (new_drive_select != ic1_drive_select)
		LOG_DEBUG(4, "DRIVE SELECT %01d, ", octet & 0x03);
	if ((octet & 0x08) != ic1_motor_enable)
		LOG_DEBUG(4, "MOTOR %s, ", (octet & 0x08)?"ON":"OFF");
	if ((octet & 0x20) != ic1_density)
		LOG_DEBUG(4, "DENSITY %s, ", (octet & 0x20)?"SINGLE":"DOUBLE");
	if ((octet & 0x10) != ic1_precomp_enable)
		LOG_DEBUG(4, "PRECOMP %s, ", (octet & 0x10)?"ON":"OFF");
	if ((octet & 0x80) != halt_enable)
		LOG_DEBUG(4, "HALT %s, ", (octet & 0x80)?"ENABLED":"DISABLED");
	LOG_DEBUG(4, "\n");
	ic1_drive_select = new_drive_select;
	vdrive_set_drive(ic1_drive_select);
	ic1_motor_enable = octet & 0x08;
	ic1_precomp_enable = octet & 0x10;
	ic1_density = octet & 0x20;
	vdrive_set_density(ic1_density);
	ic1_nmi_enable = 1;
	halt_enable = octet & 0x80;
	if (halt_enable && !(status_register & STATUS_DRQ)) {
		halt = 1;
	} else {
		halt = 0;
	}
}

void wd2797_track_register_write(unsigned int octet) {
	track_register = octet & 0xff;
}

unsigned int wd2797_track_register_read(void) {
	return track_register & 0xff;
}

void wd2797_sector_register_write(unsigned int octet) {
	sector_register = octet & 0xff;
}

unsigned int wd2797_sector_register_read(void) {
	return sector_register & 0xff;
}

void wd2797_data_register_write(unsigned int octet) {
	RESET_DRQ;
	data_register = octet & 0xff;
}

unsigned int wd2797_data_register_read(void) {
	RESET_DRQ;
	return data_register & 0xff;
}

unsigned int wd2797_status_read(void) {
	RESET_INTRQ;
	return status_register;
}

void wd2797_command_write(unsigned int cmd) {
	RESET_INTRQ;
	cmd_copy = cmd;
	/* FORCE INTERRUPT */
	if ((cmd & 0xf0) == 0xd0) {
		LOG_DEBUG(4,"WD2797: CMD: Force interrupt (%01x)\n",cmd&0x0f);
		if ((cmd & 0x0f) == 0) {
			event_dequeue(state_event);
			status_register &= ~(STATUS_BUSY);
			return;
		}
		if (cmd & 0x08) {
			event_dequeue(state_event);
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;
		}
		return;
	}
	/* Ignore any other command if busy */
	if (status_register & STATUS_BUSY) {
		LOG_DEBUG(4,"WD2797: Command received while busy!\n");
		return;
	}
	/* 0xxxxxxx = RESTORE / SEEK / STEP / STEP-IN / STEP-OUT */
	if ((cmd & 0x80) == 0x00) {
		status_register |= STATUS_BUSY;
		status_register &= ~(STATUS_CRC_ERROR|STATUS_SEEK_ERROR);
		RESET_DRQ;
		step_delay = stepping_rate[cmd & 3];
		is_step_cmd = 0;
		if ((cmd & 0xe0) == 0x20) {
			LOG_DEBUG(4, "WD2797: CMD: Step\n");
			is_step_cmd = 1;
		} else if ((cmd & 0xe0) == 0x40) {
			LOG_DEBUG(4, "WD2797: CMD: Step-in\n");
			is_step_cmd = 1;
			SET_DIRECTION;
		} else if ((cmd & 0xe0) == 0x60) {
			LOG_DEBUG(4, "WD2797: CMD: Step-out\n");
			is_step_cmd = 1;
			RESET_DIRECTION;
		}
		if (is_step_cmd) {
			if (cmd & 0x10)
				type_1_state_2();
			else
				type_1_state_3();
			return;
		}
		if ((cmd & 0xf0) == 0x00) {
			track_register = 0xff;
			data_register = 0x00;
			LOG_DEBUG(4, "WD2797: CMD: Restore\n");
		} else {
			LOG_DEBUG(4, "WD2797: CMD: Seek\n");
		}
		type_1_state_1();
		return;
	}
	/* 10xxxxxx = READ/WRITE SECTOR */
	if ((cmd & 0xc0) == 0x80) {
		if ((cmd & 0xe0) == 0x80) {
			LOG_DEBUG(4, "WD2797: CMD: Read sector (Tr %d, Side %d, Sector %d)\n", track_register, side, sector_register);
		} else {
			LOG_DEBUG(4, "WD2797: CMD: Write sector\n");
		}
		status_register |= STATUS_BUSY;
		status_register &= ~(STATUS_LOST_DATA|STATUS_RNF|(1<<5)|(1<<6));
		RESET_DRQ;
		if (!vdrive_ready) {
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;
		}
		SET_SIDE(cmd & 0x02);  /* 'U' */
		if (cmd & 0x04) {  /* 'E' set */
			NEXT_STATE(type_2_state_1, W_MILLISEC(30));
			return;
		}
		type_2_state_1();
		return;
	}
	/* 11000xx0 = READ ADDRESS */
	if ((cmd & 0xf9) == 0xc0) {
		LOG_WARN("WD2797: CMD: Read address not implemented\n");
		SET_INTRQ;
		return;
	}
	/* 11100xx0 = READ TRACK */
	if ((cmd & 0xf9) == 0xe0) {
		LOG_WARN("WD2797: CMD: Read track not implemented\n");
		SET_INTRQ;
		return;
	}
	/* 11110xx0 = WRITE TRACK */
	if ((cmd & 0xf9) == 0xf0) {
		LOG_DEBUG(4, "WD2797: CMD: Write track\n");
		status_register |= STATUS_BUSY;
		status_register &= ~(STATUS_LOST_DATA|(1<<4)|(1<<5));
		RESET_DRQ;
		if (!vdrive_ready) {
			status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;
		}
		SET_SIDE(cmd & 0x02);  /* 'U' */
		if (cmd & 0x04) {  /* 'E' set */
			NEXT_STATE(write_track_state_1, W_MILLISEC(30));
			return;
		}
		write_track_state_1();
		return;
	}
	LOG_WARN("WD2797: CMD: Unknown command %02x\n", cmd);
}

static void type_1_state_1(void) {
	if (data_register == track_register) {
		verify_track_state_1();
		return;
	}
	if (data_register > track_register)
		SET_DIRECTION;
	else
		RESET_DIRECTION;
	type_1_state_2();
}

static void type_1_state_2(void) {
	track_register += direction;
	type_1_state_3();
}

static void type_1_state_3(void) {
	if (vdrive_tr00 && direction == -1) {
		LOG_DEBUG(4,"WD2797: TR00!\n");
		track_register = 0;
		verify_track_state_1();
		return;
	}
	vdrive_step();
	if (is_step_cmd) {
		NEXT_STATE(verify_track_state_1, W_MILLISEC(step_delay));
		return;
	}
	NEXT_STATE(type_1_state_1, W_MILLISEC(step_delay));
}

static void verify_track_state_1(void) {
	if (!(cmd_copy & 0x04)) {
		status_register &= ~(STATUS_BUSY);
		SET_INTRQ;
		return;
	}
	index_holes_count = 0;
	NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
}

static void verify_track_state_2(void) {
	uint8_t *idam;
	if (vdrive_index_pulse()) {
		index_holes_count++;
		if (index_holes_count >= 5) {
			status_register &= ~(STATUS_BUSY);
			status_register |= STATUS_SEEK_ERROR;
			SET_INTRQ;
			return;
		}
	}
	idam = vdrive_next_idam();
	if (idam == NULL) {
		NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
		return;
	}
	if (track_register != idam[1]) {
		NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
		return;
	}
	if (crc16_value() != ((idam[5] << 8) | idam[6])) {
		LOG_DEBUG(3, "Verify track CRC16 error: $%04x != $%04x\n", crc16_value(), (idam[5] << 8) | idam[6]);
		status_register |= STATUS_CRC_ERROR;
		NEXT_STATE(verify_track_state_2, vdrive_time_to_next_idam());
		return;
	}
	status_register &= ~(STATUS_CRC_ERROR|STATUS_BUSY);
	SET_INTRQ;
}

static void type_2_state_1(void) {
	if ((cmd_copy & 0x20) && vdrive_write_protect) {
		status_register &= ~(STATUS_BUSY);
		status_register |= STATUS_WRITE_PROTECT;
		SET_INTRQ;
		return;
	}
	index_holes_count = 0;
	type_2_state_2();
}

static void type_2_state_2(void) {
	uint8_t *idam;
	if (vdrive_index_pulse()) {
		index_holes_count++;
		if (index_holes_count >= 5) {
			status_register &= ~(STATUS_BUSY);
			status_register |= STATUS_RNF;
			SET_INTRQ;
			return;
		}
	}
	idam = vdrive_next_idam();
	if (idam == NULL) {
		NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
		return;
	}
	if (track_register != idam[1]) {
		NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
		return;
	}
	if (sector_register != idam[3]) {
		NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
		return;
	}
	if (side != idam[2]) {
		NEXT_STATE(type_2_state_2, vdrive_time_to_next_idam());
		return;
	}
	bytes_left = sector_size[(cmd_copy & 0x08)?1:0][idam[4]&3];
	/* TODO: CRC check */
	if ((cmd_copy & 0x20) == 0) {
		unsigned int bytes_to_scan, i, tmp;
		if (IS_SINGLE_DENSITY)
			bytes_to_scan = 30;
		else
			bytes_to_scan = 43;
		i = 0;
		dam = 0;
		do {
			crc16_reset();
			tmp = vdrive_read();
			if (tmp == 0xfb || tmp == 0xf8)
				dam = tmp;
			i++;
		} while (i < bytes_to_scan && dam == 0);
		if (dam == 0) {
			NEXT_STATE(type_2_state_2, (i+1) * W_BYTE_TIME);
			return;
		}
		NEXT_STATE(read_sector_state_1, (i+1) * W_BYTE_TIME);
		return;
	}
	vdrive_skip();
	vdrive_skip();
	NEXT_STATE(write_sector_state_1, 2 * W_BYTE_TIME);
}

static void read_sector_state_1(void) {
	status_register |= ((~dam & 1) << 5);
	data_register = vdrive_read();
	bytes_left--;
	SET_DRQ;
	NEXT_STATE(read_sector_state_2, W_BYTE_TIME);
}

static void read_sector_state_2(void) {
	if (status_register & STATUS_DRQ) {
		status_register |= STATUS_LOST_DATA;
		RESET_DRQ;  // XXX
	}
	if (bytes_left > 0) {
		data_register = vdrive_read();
		bytes_left--;
		SET_DRQ;
		NEXT_STATE(read_sector_state_2, W_BYTE_TIME);
		return;
	}
	NEXT_STATE(read_sector_state_3, 2 * W_BYTE_TIME);
}

static void read_sector_state_3(void) {
	uint16_t crc = crc16_value();
	uint16_t vcrc = (vdrive_read() << 8) | vdrive_read();
	if (vcrc != crc) {
		LOG_DEBUG(3, "Read sector data tr %d se %d CRC16 error: $%04x != $%04x\n", track_register, sector_register, crc, vcrc);
		status_register |= STATUS_CRC_ERROR;
	}
	/* TODO: M == 1 */
	status_register &= ~(STATUS_BUSY);
	SET_INTRQ;
}

static void write_sector_state_1(void) {
	unsigned int i;
	SET_DRQ;
	for (i = 0; i < 8; i++)
		vdrive_skip();
	NEXT_STATE(write_sector_state_2, 8 * W_BYTE_TIME);
}

static void write_sector_state_2(void) {
	if (status_register & STATUS_DRQ) {
		status_register &= ~(STATUS_BUSY);
		RESET_DRQ;  // XXX
		status_register |= STATUS_LOST_DATA;
		SET_INTRQ;
		return;
	}
	vdrive_skip();
	NEXT_STATE(write_sector_state_3, W_BYTE_TIME);
}

static void write_sector_state_3(void) {
	unsigned int i;
	if (IS_DOUBLE_DENSITY) {
		for (i = 0; i < 11; i++)
			vdrive_skip();
		for (i = 0; i < 12; i++)
			vdrive_write(0);
		NEXT_STATE(write_sector_state_4, 23 * W_BYTE_TIME);
		return;
	}
	for (i = 0; i < 6; i++)
		vdrive_write(0);
	NEXT_STATE(write_sector_state_4, 6 * W_BYTE_TIME);
}

static void write_sector_state_4(void) {
	crc16_reset();
	if (cmd_copy & 1)
		vdrive_write(0xf8);
	else
		vdrive_write(0xfb);
	NEXT_STATE(write_sector_state_5, W_BYTE_TIME);
}

static void write_sector_state_5(void) {
	unsigned int data = data_register;
	if (status_register & STATUS_DRQ) {
		data = 0;
		status_register |= STATUS_LOST_DATA;
		RESET_DRQ;  // XXX
	}
	vdrive_write(data_register);
	bytes_left--;
	if (bytes_left > 0) {
		SET_DRQ;
		NEXT_STATE(write_sector_state_5, W_BYTE_TIME);
		return;
	}
	NEXT_STATE(write_sector_state_6, 2 * W_BYTE_TIME + W_MICROSEC(20));
}

static void write_sector_state_6(void) {
	VDRIVE_WRITE_CRC16;
	vdrive_write(0xfe);
	/* TODO: M = 1 */
	status_register &= ~(STATUS_BUSY);
	SET_INTRQ;
}

static void write_track_state_1(void) {
	if (vdrive_write_protect) {
		status_register &= ~(STATUS_BUSY);
		status_register |= STATUS_WRITE_PROTECT;
		SET_INTRQ;
		return;
	}
	SET_DRQ;
	NEXT_STATE(write_track_state_2, 3 * W_BYTE_TIME);
}

static void write_track_state_2(void) {
	unsigned int t = 0;
	if (status_register & STATUS_DRQ) {
		status_register &= ~(STATUS_BUSY);
		status_register |= STATUS_LOST_DATA;
		RESET_DRQ;  // XXX
		SET_INTRQ;
		return;
	}
	do {
		t += vdrive_time_to_next_idam();
		(void)vdrive_next_idam();
	} while (!vdrive_index_pulse());
	NEXT_STATE(write_track_state_3, t);
}

static void write_track_state_3(void) {
	unsigned int data = data_register;
	if (vdrive_index_pulse()) {
		status_register &= ~(STATUS_BUSY);
		RESET_DRQ;  // XXX
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
		//LOG_WARN("Single density write - need to implement\n");
		vdrive_write(data);
		NEXT_STATE(write_track_state_3, 2 * W_BYTE_TIME);
		return;
	}
	/* Double density */
	if (data == 0xf7) {
		VDRIVE_WRITE_CRC16;
		NEXT_STATE(write_track_state_3, 2 * W_BYTE_TIME);
		return;
	}
	if (data == 0xfe) {
		vdrive_write_idam();
		NEXT_STATE(write_track_state_3, W_BYTE_TIME);
		return;
	}
	if (data == 0xf5) {
		vdrive_write(0xa1);
		crc16_reset();
		NEXT_STATE(write_track_state_3, W_BYTE_TIME);
		return;
	}
	if (data == 0xf6) {
		data = 0xc2;
	}
	vdrive_write(data);
	NEXT_STATE(write_track_state_3, W_BYTE_TIME);
}
