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

/* The Dragon keyboard layout:
 *
 *   1   2   3   4   5   6   7   8   9   0   :   -  brk
 * up   Q   W   E   R   T   Y   U   I   O   P   @  lft rgt
 *  dwn  A   S   D   F   G   H   J   K   L   ;   enter  clr
 *  shft  Z   X   C   V   B   N   M   , .   /   shft
 *                         space
 */

/* Keymaps are lists of pairs of old keyval to new keyval
 * 'key' is the GTK keyval, 'mapping' is the Dragon key it gets mapped to.
 */

/* United Kingdom QWERTY */
static unsigned int raw_keymap_uk[] = {
	GDK_minus,       GDK_colon,
	GDK_equal,       GDK_minus,
	GDK_bracketleft, GDK_at,
	0, 0
};

/* French AZERTY */
static unsigned int raw_keymap_fr[] = {
	GDK_ampersand,   GDK_1,
	GDK_eacute,      GDK_2,
	GDK_quotedbl,    GDK_3,
	GDK_apostrophe,  GDK_4,
	GDK_parenleft,   GDK_5,
	GDK_minus,       GDK_6,
	GDK_egrave,      GDK_7,
	GDK_underscore,  GDK_8,
	GDK_ccedilla,    GDK_9,
	GDK_agrave,      GDK_0,
	GDK_parenright,  GDK_colon,
	GDK_equal,       GDK_minus,
	GDK_a,           GDK_q,
	GDK_z,           GDK_w,
	GDK_asciicircum, GDK_at,
	GDK_q,           GDK_a,
	GDK_m,           GDK_semicolon,
	GDK_w,           GDK_z,
	GDK_comma,       GDK_m,
	GDK_semicolon,   GDK_comma,
	GDK_colon,       GDK_period,
	GDK_exclam,      GDK_slash,
	0, 0
};

/* Canadian French QWERTY */
static unsigned int raw_keymap_fr_CA[] = {
	GDK_minus,           GDK_colon,
	GDK_equal,           GDK_minus,
	GDK_dead_circumflex, GDK_at,
	GDK_eacute,          GDK_slash,
	0, 0
};

/* German QWERTZ */
static unsigned int raw_keymap_de[] = {
	GDK_ssharp,     GDK_colon,
	GDK_apostrophe, GDK_minus,
	GDK_z,          GDK_y,
	GDK_udiaeresis, GDK_at,
	GDK_odiaeresis, GDK_semicolon,
	GDK_y,          GDK_z,
	GDK_minus,      GDK_slash,
	0, 0
};

static struct keymap mappings[] = {
	{ "uk", raw_keymap_uk },
	{ "us", raw_keymap_uk },
	{ "fr", raw_keymap_fr },
	{ "fr_CA", raw_keymap_fr_CA },
	{ "de", raw_keymap_de },
	{ NULL, NULL }
};
