/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2005  Ciaran Anscomb
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

/* This file is included into m6809.c for the GP32 only */

/* 0x1E EXG immediate */
case 0x1e: {
	unsigned int postbyte;
	unsigned int tmp;
	BYTE_IMMEDIATE(0,postbyte);
	switch (postbyte & 0xff) {
		case 0x01: case 0x10: tmp = reg_d; reg_d = reg_x; reg_x = tmp; break;
		case 0x02: case 0x20: tmp = reg_d; reg_d = reg_y; reg_y = tmp; break;
		case 0x03: case 0x30: tmp = reg_d; reg_d = reg_u; reg_u = tmp; break;
		case 0x04: case 0x40: tmp = reg_d; reg_d = reg_s; reg_s = tmp; break;
		case 0x05: case 0x50: tmp = reg_d; reg_d = reg_pc; reg_pc = tmp; break;
		case 0x12: case 0x21: tmp = reg_x; reg_x = reg_y; reg_y = tmp; break;
		case 0x13: case 0x31: tmp = reg_x; reg_x = reg_u; reg_u = tmp; break;
		case 0x14: case 0x41: tmp = reg_x; reg_x = reg_s; reg_s = tmp; break;
		case 0x15: case 0x51: tmp = reg_x; reg_x = reg_pc; reg_pc = tmp; break;
		case 0x23: case 0x32: tmp = reg_y; reg_y = reg_u; reg_u = tmp; break;
		case 0x24: case 0x42: tmp = reg_y; reg_y = reg_s; reg_s = tmp; break;
		case 0x25: case 0x52: tmp = reg_y; reg_y = reg_pc; reg_pc = tmp; break;
		case 0x34: case 0x43: tmp = reg_u; reg_u = reg_s; reg_s = tmp; break;
		case 0x35: case 0x53: tmp = reg_u; reg_u = reg_pc; reg_pc = tmp; break;
		case 0x45: case 0x54: tmp = reg_s; reg_s = reg_pc; reg_pc = tmp; break;
		case 0x89: case 0x98: tmp = reg_a; reg_a = reg_b; reg_b = tmp; break;
		case 0x8a: case 0xa8: tmp = reg_a; reg_a = reg_cc; reg_cc = tmp; break;
		case 0x8b: case 0xb8: tmp = reg_a; reg_a = reg_dp; reg_dp = tmp; break;
		case 0x9a: case 0xa9: tmp = reg_b; reg_b = reg_cc; reg_cc = tmp; break;
		case 0x9b: case 0xb9: tmp = reg_b; reg_b = reg_dp; reg_dp = tmp; break;
		case 0xab: case 0xba: tmp = reg_cc; reg_cc = reg_dp; reg_dp = tmp; break;
		default: break;
	}
	TAKEN_CYCLES(6);
} break;
/* 0x1F TFR immediate */
case 0x1f: {
	unsigned int postbyte;
	BYTE_IMMEDIATE(0,postbyte);
	switch (postbyte & 0xff) {
		case 0x10: reg_d = reg_x; break;
		case 0x20: reg_d = reg_y; break;
		case 0x30: reg_d = reg_u; break;
		case 0x40: reg_d = reg_s; break;
		case 0x50: reg_d = reg_pc; break;
		case 0x01: reg_x = reg_d; break;
		case 0x21: reg_x = reg_y; break;
		case 0x31: reg_x = reg_u; break;
		case 0x41: reg_x = reg_s; break;
		case 0x51: reg_x = reg_pc; break;
		case 0x02: reg_y = reg_d; break;
		case 0x12: reg_y = reg_x; break;
		case 0x32: reg_y = reg_u; break;
		case 0x42: reg_y = reg_s; break;
		case 0x52: reg_y = reg_pc; break;
		case 0x03: reg_u = reg_d; break;
		case 0x13: reg_u = reg_x; break;
		case 0x23: reg_u = reg_y; break;
		case 0x43: reg_u = reg_s; break;
		case 0x53: reg_u = reg_pc; break;
		case 0x04: reg_s = reg_d; break;
		case 0x14: reg_s = reg_x; break;
		case 0x24: reg_s = reg_y; break;
		case 0x34: reg_s = reg_u; break;
		case 0x54: reg_s = reg_pc; break;
		case 0x05: reg_pc = reg_d; break;
		case 0x15: reg_pc = reg_x; break;
		case 0x25: reg_pc = reg_y; break;
		case 0x35: reg_pc = reg_u; break;
		case 0x45: reg_pc = reg_s; break;
		case 0x98: reg_a = reg_b; break;
		case 0xa8: reg_a = reg_cc; break;
		case 0xb8: reg_a = reg_dp; break;
		case 0x89: reg_b = reg_a; break;
		case 0xa9: reg_b = reg_cc; break;
		case 0xb9: reg_b = reg_dp; break;
		case 0x8a: reg_cc = reg_a; break;
		case 0x9a: reg_cc = reg_b; break;
		case 0xba: reg_cc = reg_dp; break;
		case 0x8b: reg_dp = reg_a; break;
		case 0x9b: reg_dp = reg_b; break;
		case 0xab: reg_dp = reg_cc; break;
		default: break;
	}
	TAKEN_CYCLES(4);
} break;
