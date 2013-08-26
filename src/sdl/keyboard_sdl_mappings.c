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

/* Keymaps are lists of pairs of 'key' to 'mapping'.
 * 'key' is the SDL keysym, 'mapping' is the Dragon key it gets mapped to.
 */

/* United Kingdom QWERTY */
static unsigned int raw_keymap_uk[] = {
	'-',  ':',
	'=',  '-',
	'[',  '@',
	0, 0
};

/* French AZERTY */
static unsigned int raw_keymap_fr[] = {
	'&',  '1',
	SDLK_WORLD_73, '2',
	'"',  '3',
	'\'', '4',
	'(',  '5',
	'-',  '6',
	SDLK_WORLD_72, '7',
	'_',  '8',
	SDLK_WORLD_71, '9',
	SDLK_WORLD_64, '0',
	')',  ':',
	'=',  '-',
	'a',  'q',
	'z',  'w',
	'^',  '@',
	'q',  'a',
	'm',  ';',
	'w',  'z',
	',',  'm',
	';',  ',',
	':',  '.',
	'!',  '/',
	0, 0
};

/* Canadian French QWERTY */
static unsigned int raw_keymap_fr_CA[] = {
	'-',  ':',
	'=',  '-',
	SDLK_CARET,    '@',  /* key at right of P */
	SDLK_WORLD_73, '/',  /* key at right of period */
	0, 0
};

/* German QWERTZ */
static unsigned int raw_keymap_de[] = {
	SDLK_WORLD_63, ':',
	'\'', '-',
	'z',  'y',
	SDLK_WORLD_92, '@',
	SDLK_WORLD_86, ';',
	'y',  'z',
	'-',  '/',
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
