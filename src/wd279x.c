/*  Copyright 2003-2013 Ciaran Anscomb
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

/* Sources:
 *     WD279x:
 *         http://www.swtpc.com/mholley/DC_5/TMS279X_DataSheet.pdf
 */

#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pl_glib.h"

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
		fdc->status_register |= STATUS_DRQ; \
		if (fdc->set_drq_handler) \
			fdc->set_drq_handler(fdc->drq_data); \
	} while (0)
#define RESET_DRQ do { \
		fdc->status_register &= ~(STATUS_DRQ); \
		if (fdc->reset_drq_handler) \
			fdc->reset_drq_handler(fdc->drq_data); \
	} while (0)
#define SET_INTRQ do { \
		if (fdc->set_intrq_handler) \
			fdc->set_intrq_handler(fdc->intrq_data); \
	} while (0)
#define RESET_INTRQ do { \
		if (fdc->reset_intrq_handler) \
			fdc->reset_intrq_handler(fdc->intrq_data); \
	} while (0)

#define NEXT_STATE(f,t) do { \
		fdc->state = f; \
		fdc->state_event.at_tick = event_current_tick + t; \
		event_queue(&MACHINE_EVENT_LIST, &fdc->state_event); \
	} while (0)
#define GOTO_STATE(f) fdc->state = f; continue

#define IS_DOUBLE_DENSITY (fdc->double_density)
#define IS_SINGLE_DENSITY (!fdc->double_density)

#define SET_DIRECTION do { fdc->direction = 1; vdrive_set_direction(1); } while (0)
#define RESET_DIRECTION do { \
		fdc->direction = -1; vdrive_set_direction(-1); \
	} while (0)
#define SET_SIDE(s) do { \
		fdc->side = (s) ? 1 : 0; \
		if (fdc->has_sso) \
			vdrive_set_head(fdc->side); \
	} while (0)

static void state_machine(WD279X *fdc);

static int stepping_rate[4] = { 6, 12, 20, 30 };
static int sector_size[2][4] = {
	{ 256, 512, 1024, 128 },
	{ 128, 256, 512, 1024 }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Debugging

struct log_handle *log_rsec_hex = NULL;
struct log_handle *log_wsec_hex = NULL;
struct log_handle *log_wtrk_hex = NULL;

static void debug_state(WD279X *fdc);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static uint8_t _vdrive_read(WD279X *fdc) {
	uint8_t b = vdrive_read();
	fdc->crc = crc16_byte(fdc->crc, b);
	return b;
}

static void _vdrive_write(WD279X *fdc, uint8_t b) {
	vdrive_write(b);
	fdc->crc = crc16_byte(fdc->crc, b);
}

#define VDRIVE_WRITE_CRC16 do { \
		uint16_t tmp = fdc->crc; \
		_vdrive_write(fdc, tmp >> 8); \
		_vdrive_write(fdc, tmp & 0xff); \
	} while (0)

static void wd279x_init(WD279X *fdc, enum WD279X_type type) {
	fdc->type = type;
	fdc->has_sso = (type == WD2795 || type == WD2797);
	fdc->has_length_flag = (type == WD2795 || type == WD2797);
	fdc->has_inverted_data = (type == WD2791 || type == WD2795);
	fdc->set_drq_handler = NULL;
	fdc->reset_drq_handler = NULL;
	fdc->set_intrq_handler = NULL;
	fdc->reset_intrq_handler = NULL;
	fdc->state = WD279X_state_accept_command;
	event_init(&fdc->state_event, (event_delegate)state_machine, fdc);
}

WD279X *wd279x_new(enum WD279X_type type) {
	WD279X *new = g_malloc(sizeof(WD279X));
	wd279x_init(new, type);
	return new;
}

static void wd279x_deinit(WD279X *fdc) {
	event_dequeue(&fdc->state_event);
}

void wd279x_free(WD279X *fdc) {
	assert(fdc != NULL);
	wd279x_deinit(fdc);
	g_free(fdc);
}

void wd279x_reset(WD279X *fdc) {
	assert(fdc != NULL);
	event_dequeue(&fdc->state_event);
	fdc->status_register = 0;
	fdc->track_register = 0;
	fdc->sector_register = 0;
	fdc->data_register = 0;
	fdc->command_register = 0;
	RESET_DIRECTION;
	SET_SIDE(0);
}

void wd279x_set_dden(WD279X *fdc, _Bool dden) {
	fdc->double_density = dden;
	vdrive_set_dden(dden);
}

uint8_t wd279x_read(WD279X *fdc, uint16_t A) {
	uint8_t D;
	switch (A & 3) {
		default:
		case 0:
			RESET_INTRQ;
			if (vdrive_ready)
				fdc->status_register &= ~STATUS_NOT_READY;
			else
				fdc->status_register |= STATUS_NOT_READY;
			if ((fdc->command_register & 0xf0) == 0xd0 || (fdc->command_register & 0x80) == 0x00) {
				if (vdrive_tr00)
					fdc->status_register |= STATUS_TRACK_0;
				else
					fdc->status_register &= ~STATUS_TRACK_0;
				if (vdrive_index_pulse)
					fdc->status_register |= STATUS_INDEX_PULSE;
				else
					fdc->status_register &= ~STATUS_INDEX_PULSE;
			}
			D = fdc->status_register;
			break;
		case 1:
			D = fdc->track_register;
			break;
		case 2:
			D = fdc->sector_register;
			break;
		case 3:
			RESET_DRQ;
			D = fdc->data_register;
			break;
	}
	if (fdc->has_inverted_data)
		return ~D;
	return D;
}

void wd279x_write(WD279X *fdc, uint16_t A, uint8_t D) {
	if (fdc->has_inverted_data)
		D = ~D;
	switch (A & 3) {
		default:
		case 0:
			RESET_INTRQ;
			fdc->command_register = D;
			/* FORCE INTERRUPT */
			if ((fdc->command_register & 0xf0) == 0xd0) {
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_STATE) {
					debug_state(fdc);
				}
				if ((fdc->command_register & 0x0f) == 0) {
					event_dequeue(&fdc->state_event);
					fdc->status_register &= ~(STATUS_BUSY);
					return;
				}
				if (fdc->command_register & 0x08) {
					event_dequeue(&fdc->state_event);
					fdc->status_register &= ~(STATUS_BUSY);
					SET_INTRQ;
					return;
				}
				return;
			}
			/* Ignore any other command if busy */
			if (fdc->status_register & STATUS_BUSY) {
				LOG_DEBUG(4,"WD279X: Command received while busy!\n");
				return;
			}
			fdc->state = WD279X_state_accept_command;
			state_machine(fdc);
			break;
		case 1:
			fdc->track_register = D;
			break;
		case 2:
			fdc->sector_register = D;
			break;
		case 3:
			RESET_DRQ;
			fdc->data_register = D;
			break;
	}
}

/* One big state machine.  This is called from an event dispatch and from the
 * write command function. */

static void state_machine(WD279X *fdc) {
	uint8_t *idam;
	uint8_t data;
	int i;
	for (;;) {

		// Log new states if requested:
		if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_STATE) {
			static enum WD279X_state last_state = WD279X_state_invalid;
			if (fdc->state != last_state) {
				debug_state(fdc);
				last_state = fdc->state;
			}
		}

		switch (fdc->state) {

		case WD279X_state_accept_command:
			/* 0xxxxxxx = RESTORE / SEEK / STEP / STEP-IN / STEP-OUT */
			if ((fdc->command_register & 0x80) == 0x00) {
				fdc->status_register |= STATUS_BUSY;
				fdc->status_register &= ~(STATUS_CRC_ERROR|STATUS_SEEK_ERROR);
				RESET_DRQ;
				fdc->step_delay = stepping_rate[fdc->command_register & 3];
				fdc->is_step_cmd = 0;
				if ((fdc->command_register & 0xe0) == 0x20) {
					fdc->is_step_cmd = 1;
				} else if ((fdc->command_register & 0xe0) == 0x40) {
					fdc->is_step_cmd = 1;
					SET_DIRECTION;
				} else if ((fdc->command_register & 0xe0) == 0x60) {
					fdc->is_step_cmd = 1;
					RESET_DIRECTION;
				}
				if (fdc->is_step_cmd) {
					if (fdc->command_register & 0x10) {
						GOTO_STATE(WD279X_state_type1_2);
					}
					GOTO_STATE(WD279X_state_type1_3);
				}
				if ((fdc->command_register & 0xf0) == 0x00) {
					fdc->track_register = 0xff;
					fdc->data_register = 0x00;
				}
				GOTO_STATE(WD279X_state_type1_1);
			}

			/* 10xxxxxx = READ/WRITE SECTOR */
			if ((fdc->command_register & 0xc0) == 0x80) {
				fdc->status_register |= STATUS_BUSY;
				fdc->status_register &= ~(STATUS_LOST_DATA|STATUS_RNF|(1<<5)|(1<<6));
				RESET_DRQ;
				if (!vdrive_ready) {
					fdc->status_register &= ~(STATUS_BUSY);
					SET_INTRQ;
					return;
				}
				if (fdc->has_sso)
					SET_SIDE(fdc->command_register & 0x02);  /* 'U' */
				else
					SET_SIDE(fdc->command_register & 0x08);  /* 'S' */
				if (fdc->command_register & 0x04) {  /* 'E' set */
					NEXT_STATE(WD279X_state_type2_1, W_MILLISEC(30));
					return;
				}
				GOTO_STATE(WD279X_state_type2_1);
			}

			/* 11000xx0 = READ ADDRESS */
			/* 11100xx0 = READ TRACK */
			/* 11110xx0 = WRITE TRACK */
			if (((fdc->command_register & 0xf9) == 0xc0)
					|| ((fdc->command_register & 0xf9) == 0xe0)
					|| ((fdc->command_register & 0xf9) == 0xf0)) {
				fdc->status_register |= STATUS_BUSY;
				fdc->status_register &= ~(STATUS_LOST_DATA|(1<<4)|(1<<5));
				if ((fdc->command_register & 0xf0) == 0xf0)
					RESET_DRQ;
				if (!vdrive_ready) {
					fdc->status_register &= ~(STATUS_BUSY);
					SET_INTRQ;
					return;
				}
				if (fdc->has_sso)
					SET_SIDE(fdc->command_register & 0x02);  /* 'U' */
				else
					SET_SIDE(fdc->command_register & 0x08);  /* 'S' */
				if (fdc->command_register & 0x04) {  /* 'E' set */
					NEXT_STATE(WD279X_state_type3_1, W_MILLISEC(30));
					return;
				}
				GOTO_STATE(WD279X_state_type3_1);
			}
			LOG_WARN("WD279X: CMD: Unknown command %02x\n", fdc->command_register);
			return;


		case WD279X_state_type1_1:
			if (fdc->data_register == fdc->track_register) {
				GOTO_STATE(WD279X_state_verify_track_1);
			}
			if (fdc->data_register > fdc->track_register)
				SET_DIRECTION;
			else
				RESET_DIRECTION;
			GOTO_STATE(WD279X_state_type1_2);


		case WD279X_state_type1_2:
			fdc->track_register += fdc->direction;
			GOTO_STATE(WD279X_state_type1_3);


		case WD279X_state_type1_3:
			if (vdrive_tr00 && fdc->direction == -1) {
				LOG_DEBUG(4,"WD279X: TR00!\n");
				fdc->track_register = 0;
				// The WD279x flow chart implies this delay is
				// not incurred in this situation, but real
				// code fails without it.
				NEXT_STATE(WD279X_state_verify_track_1, W_MILLISEC(fdc->step_delay));
				return;
			}
			vdrive_step();
			if (fdc->is_step_cmd) {
				NEXT_STATE(WD279X_state_verify_track_1, W_MILLISEC(fdc->step_delay));
				return;
			}
			NEXT_STATE(WD279X_state_type1_1, W_MILLISEC(fdc->step_delay));
			return;


		case WD279X_state_verify_track_1:
			if (!(fdc->command_register & 0x04)) {
				fdc->status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			fdc->index_holes_count = 0;
			NEXT_STATE(WD279X_state_verify_track_2, vdrive_time_to_next_idam());
			return;


		case WD279X_state_verify_track_2:
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				fdc->index_holes_count++;
				if (fdc->index_holes_count >= 5) {
					LOG_DEBUG(5, "index_holes_count >= 5: seek error\n");
					fdc->status_register &= ~(STATUS_BUSY);
					fdc->status_register |= STATUS_SEEK_ERROR;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				LOG_DEBUG(5, "null IDAM: -> WD279X_state_verify_track_2\n");
				NEXT_STATE(WD279X_state_verify_track_2, vdrive_time_to_next_idam());
				return;
			}
			fdc->crc = CRC16_RESET;
			if (IS_DOUBLE_DENSITY) {
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
			}
			(void)_vdrive_read(fdc);  /* Include IDAM in CRC */
			if (fdc->track_register != _vdrive_read(fdc)) {
				LOG_DEBUG(5, "track_register != idam[1]: -> WD279X_state_verify_track_2\n");
				NEXT_STATE(WD279X_state_verify_track_2, vdrive_time_to_next_idam());
				return;
			}
			/* Include rest of ID field - should result in computed CRC = 0 */
			for (i = 0; i < 5; i++)
				(void)_vdrive_read(fdc);
			if (fdc->crc != 0) {
				LOG_DEBUG(3, "Verify track %d CRC16 error: $%04x != 0\n", fdc->track_register, fdc->crc);
				fdc->status_register |= STATUS_CRC_ERROR;
				NEXT_STATE(WD279X_state_verify_track_2, vdrive_time_to_next_idam());
				return;
			}
			LOG_DEBUG(5, "finished.\n");
			fdc->status_register &= ~(STATUS_CRC_ERROR|STATUS_BUSY);
			SET_INTRQ;
			return;


		case WD279X_state_type2_1:
			if ((fdc->command_register & 0x20) && vdrive_write_protect) {
				fdc->status_register &= ~(STATUS_BUSY);
				fdc->status_register |= STATUS_WRITE_PROTECT;
				SET_INTRQ;
				return;
			}
			fdc->index_holes_count = 0;
			NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
			return;


		case WD279X_state_type2_2:
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				fdc->index_holes_count++;
				if (fdc->index_holes_count >= 5) {
					fdc->status_register &= ~(STATUS_BUSY);
					fdc->status_register |= STATUS_RNF;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
				return;
			}
			fdc->crc = CRC16_RESET;
			if (IS_DOUBLE_DENSITY) {
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
			}
			(void)_vdrive_read(fdc);  /* Include IDAM in CRC */
			if (fdc->track_register != _vdrive_read(fdc)) {
				NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
				return;
			}
			if (fdc->side != (int)_vdrive_read(fdc)) {
				/* No error if no SSO or 'C' not set */
				if (fdc->has_sso || fdc->command_register & 0x02) {
					NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
					return;
				}
			}
			if (fdc->sector_register != _vdrive_read(fdc)) {
				NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
				return;
			}
			i = _vdrive_read(fdc);
			if (fdc->has_length_flag)
				fdc->bytes_left = sector_size[(fdc->command_register & 0x08)?1:0][i&3];
			else
				fdc->bytes_left = sector_size[1][i&3];
			/* Including CRC bytes should result in computed CRC = 0 */
			(void)_vdrive_read(fdc);
			(void)_vdrive_read(fdc);
			if (fdc->crc != 0) {
				fdc->status_register |= STATUS_CRC_ERROR;
				LOG_DEBUG(3, "Type 2 tr %d se %d CRC16 error: $%04x != 0\n", fdc->track_register, fdc->sector_register, fdc->crc);
				NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_idam());
				return;
			}

			if ((fdc->command_register & 0x20) == 0) {
				int bytes_to_scan, j, tmp;
				if (IS_SINGLE_DENSITY)
					bytes_to_scan = 30;
				else
					bytes_to_scan = 43;
				j = 0;
				fdc->dam = 0;
				do {
					fdc->crc = CRC16_RESET;
					if (IS_DOUBLE_DENSITY) {
						fdc->crc = crc16_byte(fdc->crc, 0xa1);
						fdc->crc = crc16_byte(fdc->crc, 0xa1);
						fdc->crc = crc16_byte(fdc->crc, 0xa1);
					}
					tmp = _vdrive_read(fdc);
					if (tmp == 0xfb || tmp == 0xf8)
						fdc->dam = tmp;
					j++;
				} while (j < bytes_to_scan && fdc->dam == 0);
				if (fdc->dam == 0) {
					NEXT_STATE(WD279X_state_type2_2, vdrive_time_to_next_byte());
					return;
				}
				NEXT_STATE(WD279X_state_read_sector_1, vdrive_time_to_next_byte());
				return;
			}
			vdrive_skip();
			vdrive_skip();
			NEXT_STATE(WD279X_state_write_sector_1, vdrive_time_to_next_byte());
			return;


		case WD279X_state_read_sector_1:
			LOG_DEBUG(4,"Reading %d-byte sector (Tr %d, Se %d) from head_pos=%04x\n", fdc->bytes_left, fdc->track_register, fdc->sector_register, vdrive_head_pos());
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_open_hexdump(&log_rsec_hex, "WD279X: read-sector");
			fdc->status_register |= ((~fdc->dam & 1) << 5);
			fdc->data_register = _vdrive_read(fdc);
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_hexdump_byte(log_rsec_hex, fdc->data_register);
			fdc->bytes_left--;
			SET_DRQ;
			NEXT_STATE(WD279X_state_read_sector_2, vdrive_time_to_next_byte());
			return;


		case WD279X_state_read_sector_2:
			if (fdc->status_register & STATUS_DRQ) {
				fdc->status_register |= STATUS_LOST_DATA;
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
					log_hexdump_flag(log_rsec_hex);
				/* RESET_DRQ;  XXX */
			}
			if (fdc->bytes_left > 0) {
				fdc->data_register = _vdrive_read(fdc);
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
					log_hexdump_byte(log_rsec_hex, fdc->data_register);
				fdc->bytes_left--;
				SET_DRQ;
				NEXT_STATE(WD279X_state_read_sector_2, vdrive_time_to_next_byte());
				return;
			}
			log_close(&log_rsec_hex);
			/* Including CRC bytes should result in computed CRC = 0 */
			(void)_vdrive_read(fdc);
			(void)_vdrive_read(fdc);
			NEXT_STATE(WD279X_state_read_sector_3, vdrive_time_to_next_byte());
			return;


		case WD279X_state_read_sector_3:
			if (fdc->crc != 0) {
				LOG_DEBUG(3, "Read sector data tr %d se %d CRC16 error: $%04x != 0\n", fdc->track_register, fdc->sector_register, fdc->crc);
				fdc->status_register |= STATUS_CRC_ERROR;
			}
			/* TODO: M == 1 */
			if (fdc->command_register & 0x10) {
				LOG_DEBUG(2, "WD279X: TODO: multi-sector read will fail.\n");
			}
			fdc->status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case WD279X_state_write_sector_1:
			SET_DRQ;
			for (i = 0; i < 8; i++)
				vdrive_skip();
			NEXT_STATE(WD279X_state_write_sector_2, vdrive_time_to_next_byte());
			return;


		case WD279X_state_write_sector_2:
			if (fdc->status_register & STATUS_DRQ) {
				fdc->status_register &= ~(STATUS_BUSY);
				RESET_DRQ;  /* XXX */
				fdc->status_register |= STATUS_LOST_DATA;
				SET_INTRQ;
				return;
			}
			vdrive_skip();
			NEXT_STATE(WD279X_state_write_sector_3, vdrive_time_to_next_byte());
			return;


		case WD279X_state_write_sector_3:
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_open_hexdump(&log_wsec_hex, "WD279X: write-sector");
			if (IS_DOUBLE_DENSITY) {
				for (i = 0; i < 11; i++)
					vdrive_skip();
				for (i = 0; i < 12; i++)
					_vdrive_write(fdc, 0);
				NEXT_STATE(WD279X_state_write_sector_4, vdrive_time_to_next_byte());
				return;
			}
			for (i = 0; i < 6; i++)
				_vdrive_write(fdc, 0);
			NEXT_STATE(WD279X_state_write_sector_4, vdrive_time_to_next_byte());
			return;


		case WD279X_state_write_sector_4:
			fdc->crc = CRC16_RESET;
			if (IS_DOUBLE_DENSITY) {
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
			}
			if (fdc->command_register & 1)
				_vdrive_write(fdc, 0xf8);
			else
				_vdrive_write(fdc, 0xfb);
			NEXT_STATE(WD279X_state_write_sector_5, vdrive_time_to_next_byte());
			return;


		case WD279X_state_write_sector_5:
			data = fdc->data_register;
			if (fdc->status_register & STATUS_DRQ) {
				data = 0;
				fdc->status_register |= STATUS_LOST_DATA;
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
					log_hexdump_flag(log_wsec_hex);
				RESET_DRQ;  /* XXX */
			}
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_hexdump_byte(log_wsec_hex, data);
			_vdrive_write(fdc, data);
			fdc->bytes_left--;
			if (fdc->bytes_left > 0) {
				SET_DRQ;
				NEXT_STATE(WD279X_state_write_sector_5, vdrive_time_to_next_byte());
				return;
			}
			log_close(&log_wsec_hex);
			VDRIVE_WRITE_CRC16;
			NEXT_STATE(WD279X_state_write_sector_6, vdrive_time_to_next_byte() + W_MICROSEC(20));
			return;


		case WD279X_state_write_sector_6:
			_vdrive_write(fdc, 0xfe);
			/* TODO: M = 1 */
			fdc->status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case WD279X_state_type3_1:
			switch (fdc->command_register & 0xf0) {
				case 0xc0:
					fdc->index_holes_count = 0;
					NEXT_STATE(WD279X_state_read_address_1, vdrive_time_to_next_idam());
					return;
				case 0xe0:
					LOG_WARN("WD279X: CMD: Read track not implemented\n");
					SET_INTRQ;
					break;
				case 0xf0:
					GOTO_STATE(WD279X_state_write_track_1);
				default:
					break;
			}
			return;


		case WD279X_state_read_address_1:
			idam = vdrive_next_idam();
			if (vdrive_new_index_pulse()) {
				fdc->index_holes_count++;
				if (fdc->index_holes_count >= 6) {
					fdc->status_register &= ~(STATUS_BUSY);
					fdc->status_register |= STATUS_RNF;
					SET_INTRQ;
					return;
				}
			}
			if (idam == NULL) {
				NEXT_STATE(WD279X_state_read_address_1, vdrive_time_to_next_idam());
				return;
			}
			fdc->crc = CRC16_RESET;
			if (IS_DOUBLE_DENSITY) {
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
			}
			(void)_vdrive_read(fdc);
			NEXT_STATE(WD279X_state_read_address_2, vdrive_time_to_next_byte());
			return;


		case WD279X_state_read_address_2:
			fdc->bytes_left = 5;
			fdc->data_register = _vdrive_read(fdc);
			/* At end of command, this is transferred to the sector register: */
			fdc->track_register_tmp = fdc->data_register;
			SET_DRQ;
			NEXT_STATE(WD279X_state_read_address_3, vdrive_time_to_next_byte());
			return;


		case WD279X_state_read_address_3:
			/* Lost data not mentioned in data sheet, so not checking
			   for now */
			if (fdc->bytes_left > 0) {
				fdc->data_register = _vdrive_read(fdc);
				fdc->bytes_left--;
				SET_DRQ;
				NEXT_STATE(WD279X_state_read_address_3, vdrive_time_to_next_byte());
				return;
			}
			fdc->sector_register = fdc->track_register_tmp;
			if (fdc->crc != 0) {
				fdc->status_register |= STATUS_CRC_ERROR;
			}
			fdc->status_register &= ~(STATUS_BUSY);
			SET_INTRQ;
			return;


		case WD279X_state_write_track_1:
			if (vdrive_write_protect) {
				fdc->status_register &= ~(STATUS_BUSY);
				fdc->status_register |= STATUS_WRITE_PROTECT;
				SET_INTRQ;
				return;
			}
			SET_DRQ;
			/* Data sheet says 3 byte times, but CoCo NitrOS9 fails unless I set
			 * this delay higher. */
			NEXT_STATE(WD279X_state_write_track_2, 6 * W_BYTE_TIME);
			return;


		case WD279X_state_write_track_2:
			if (fdc->status_register & STATUS_DRQ) {
				RESET_DRQ;  /* XXX */
				fdc->status_register |= STATUS_LOST_DATA;
				fdc->status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			NEXT_STATE(WD279X_state_write_track_2b, vdrive_time_to_next_idam());
			return;


		case WD279X_state_write_track_2b:
			if (!vdrive_new_index_pulse()) {
				LOG_DEBUG(4,"Waiting for index pulse, head_pos=%04x\n", vdrive_head_pos());
				NEXT_STATE(WD279X_state_write_track_2b, vdrive_time_to_next_idam());
				return;
			}
			LOG_DEBUG(4,"Writing track from head_pos=%04x\n", vdrive_head_pos());
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_open_hexdump(&log_wtrk_hex, "WD279X: write-track");
			GOTO_STATE(WD279X_state_write_track_3);


		case WD279X_state_write_track_3:
			data = fdc->data_register;
			if (vdrive_new_index_pulse()) {
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
					log_close(&log_wtrk_hex);
				LOG_DEBUG(4,"Finished writing track at head_pos=%04x\n", vdrive_head_pos());
				RESET_DRQ;  /* XXX */
				fdc->status_register &= ~(STATUS_BUSY);
				SET_INTRQ;
				return;
			}
			if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
				log_hexdump_byte(log_wtrk_hex, fdc->data_register);
			if (fdc->status_register & STATUS_DRQ) {
				data = 0;
				fdc->status_register |= STATUS_LOST_DATA;
				if (xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_DATA)
					log_hexdump_flag(log_wtrk_hex);
			}
			SET_DRQ;
			if (IS_SINGLE_DENSITY) {
				/* Single density */
				if (data == 0xf5 || data == 0xf6) {
					LOG_DEBUG(4, "Illegal value in single-density track write: %02x\n", data);
				}
				if (data == 0xf7) {
					VDRIVE_WRITE_CRC16;
					NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
					return;
				}
				if (data >= 0xf8 && data <= 0xfb) {
					fdc->crc = CRC16_RESET;
					_vdrive_write(fdc, data);
					NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
					return;
				}
				if (data == 0xfe) {
					LOG_DEBUG(4,"IDAM at head_pos=%04x\n", vdrive_head_pos());
					fdc->crc = CRC16_RESET;
					vdrive_write_idam();
					fdc->crc = crc16_byte(fdc->crc, 0xfe);
					NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
					return;
				}
				_vdrive_write(fdc, data);
				NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
				return;
			}
			/* Double density */
			if (data == 0xf7) {
				VDRIVE_WRITE_CRC16;
				NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xfe) {
				LOG_DEBUG(4,"IDAM at head_pos=%04x\n", vdrive_head_pos());
				vdrive_write_idam();
				fdc->crc = crc16_byte(fdc->crc, 0xfe);
				NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xf5) {
				fdc->crc = CRC16_RESET;
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				fdc->crc = crc16_byte(fdc->crc, 0xa1);
				_vdrive_write(fdc, 0xa1);
				NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
				return;
			}
			if (data == 0xf6) {
				data = 0xc2;
			}
			_vdrive_write(fdc, data);
			NEXT_STATE(WD279X_state_write_track_3, vdrive_time_to_next_byte());
			return;

		default:
			return;

		}
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Debugging

static const char *debug_state_name[] = {
	"accept_command",
	"type1_1",
	"type1_2",
	"type1_3",
	"verify_track_1",
	"verify_track_2",
	"type2_1",
	"type2_2",
	"read_sector_1",
	"read_sector_2",
	"read_sector_3",
	"write_sector_1",
	"write_sector_2",
	"write_sector_3",
	"write_sector_4",
	"write_sector_5",
	"write_sector_6",
	"type3_1",
	"read_address_1",
	"read_address_2",
	"read_address_3",
	"write_track_1",
	"write_track_2",
	"write_track_2b",
	"write_track_3",
};

static const char *debug_command[] = {
	"restore",
	"seek",
	"step",
	"step",
	"step-in",
	"step-in",
	"step-out",
	"step-out",
	"read-sector",
	"read-sector",
	"write-sector",
	"write-sector",
	"read-address",
	"force-interrupt",
	"read-track N/A",
	"write-track",
};

static void debug_state(WD279X *fdc) {
	assert(fdc != NULL);
	assert((unsigned)fdc->state < WD279X_state_invalid);
	unsigned level = xroar_cfg.debug_fdc & XROAR_DEBUG_FDC_STATE;
	if (level == 0)
		return;
	_Bool forced_interrupt = ((fdc->command_register & 0xf0) == 0xd0);
	if (fdc->state <= WD279X_state_accept_command || forced_interrupt) {
		// command (incl. forced interrupt)
		unsigned type = ((fdc->command_register) >> 4) & 15;
		LOG_PRINT("WD279X: CR=%02x ST=%02x TR=%02x SR=%02x DR=%02x state=%s [%s]\n", fdc->command_register, fdc->status_register, fdc->track_register, fdc->sector_register, fdc->data_register, debug_state_name[fdc->state], debug_command[type]);
	} else if (level >= 2) {
		// any other state
		LOG_PRINT("WD279X: CR=%02x ST=%02x TR=%02x SR=%02x DR=%02x state=%s\n", fdc->command_register, fdc->status_register, fdc->track_register, fdc->sector_register, fdc->data_register, debug_state_name[fdc->state]);
	}
}
