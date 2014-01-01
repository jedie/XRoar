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

/* The Dragon keyboard layout:
 *
 *   1   2   3   4   5   6   7   8   9   0   :   -  brk
 * up   Q   W   E   R   T   Y   U   I   O   P   @  lft rgt
 *  dwn  A   S   D   F   G   H   J   K   L   ;   enter  clr
 *  shft  Z   X   C   V   B   N   M   , .   /   shft
 *                         space
 */

/* Keymaps map SDL keysym to dkey. */

/* uk, United Kingdom, QWERTY */
/* cymru, Welsh, QWERTY */
/* eng, English, QWERTY */
/* scot, Scottish, QWERTY */
/* ie, Irish, QWERTY */
/* usa, USA, QWERTY */
static struct sym_dkey_mapping keymap_uk[] = {
	{ SDLK_MINUS, DSCAN_COLON },
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_LEFTBRACKET, DSCAN_AT },
	{ SDLK_SEMICOLON, DSCAN_SEMICOLON },
	{ SDLK_BACKQUOTE, DSCAN_CLEAR, 1 },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_SLASH, DSCAN_SLASH },
};

/* be, Belgian, AZERTY */
static struct sym_dkey_mapping keymap_be[] = {
	{ SDLK_AMPERSAND, DSCAN_1 },
	{ SDLK_QUOTEDBL, DSCAN_3 },
	{ SDLK_QUOTE, DSCAN_4 },
	{ SDLK_LEFTPAREN, DSCAN_5 },
	{ SDLK_EXCLAIM, DSCAN_8 },
	{ SDLK_RIGHTPAREN, DSCAN_COLON },
	{ SDLK_MINUS, DSCAN_MINUS },
	{ SDLK_a, DSCAN_Q },
	{ SDLK_z, DSCAN_W },
	{ SDLK_CARET, DSCAN_AT },
	{ SDLK_q, DSCAN_A },
	{ SDLK_m, DSCAN_SEMICOLON },
	{ SDLK_w, DSCAN_Z },
	{ SDLK_COMMA, DSCAN_M },
	{ SDLK_SEMICOLON, DSCAN_COMMA },
	{ SDLK_COLON, DSCAN_FULL_STOP },
	{ SDLK_EQUALS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_WORLD_73, DSCAN_2 }, // é
	{ SDLK_WORLD_7, DSCAN_6 }, // §
	{ SDLK_WORLD_72, DSCAN_7 }, // è
	{ SDLK_WORLD_71, DSCAN_9 }, // ç
	{ SDLK_WORLD_64, DSCAN_0 }, // à
	{ SDLK_WORLD_18, DSCAN_CLEAR, 1 }, // ²
#else
	{ SDLK_WORLD_0, DSCAN_2 }, // é
	{ SDLK_WORLD_1, DSCAN_6 }, // §
	{ SDLK_WORLD_3, DSCAN_7 }, // è
	{ SDLK_WORLD_2, DSCAN_9 }, // ç
	{ SDLK_WORLD_4, DSCAN_0 }, // à
	{ SDLK_BACKQUOTE, DSCAN_CLEAR, 1 },
#endif
};

/* de, German, QWERTZ */
static struct sym_dkey_mapping keymap_de[] = {
	{ SDLK_z, DSCAN_Y },
	{ SDLK_y, DSCAN_Z },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_WORLD_63, DSCAN_COLON }, // ß
	{ SDLK_COMPOSE, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_92, DSCAN_AT }, // ü
	{ SDLK_WORLD_86, DSCAN_SEMICOLON }, // ö
	{ SDLK_CARET, DSCAN_CLEAR, 1 }, // dead caret
#else
	{ SDLK_WORLD_1, DSCAN_COLON }, // ß
	{ SDLK_WORLD_0, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_2, DSCAN_AT }, // ü
	{ SDLK_WORLD_4, DSCAN_SEMICOLON }, // ö
	{ SDLK_WORLD_3, DSCAN_CLEAR, 1 }, // ä
#endif
};

/* dk, Danish, QWERTY */
static struct sym_dkey_mapping keymap_dk[] = {
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_COMPOSE, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_69, DSCAN_AT }, // å
	{ SDLK_WORLD_70, DSCAN_SEMICOLON }, // æ
	{ SDLK_WORLD_29, DSCAN_CLEAR, 1 }, // ½
	{ SDLK_WORLD_88, DSCAN_CLEAR, 1 }, // ø
#else
	{ SDLK_WORLD_0, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_2, DSCAN_AT }, // å
	{ SDLK_WORLD_4, DSCAN_SEMICOLON }, // æ
	{ SDLK_WORLD_3, DSCAN_CLEAR, 1 }, // ø
#endif
};

/* es, Spanish, QWERTY */
static struct sym_dkey_mapping keymap_es[] = {
	{ SDLK_QUOTE, DSCAN_COLON },
	{ SDLK_WORLD_1, DSCAN_MINUS },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_COMPOSE, DSCAN_AT }, // dead grave
	{ SDLK_WORLD_81, DSCAN_SEMICOLON }, // ñ
	{ SDLK_WORLD_26, DSCAN_CLEAR, 1 }, // º
#else
	{ SDLK_BACKQUOTE, DSCAN_AT },
	{ SDLK_WORLD_3, DSCAN_SEMICOLON },
	{ SDLK_WORLD_2, DSCAN_CLEAR, 1 }, // dead acute
#endif
};

#ifdef HAVE_COCOA
/* es, Spanish, QWERTY (Apple) */
static struct sym_dkey_mapping keymap_es_apple[] = {
	{ SDLK_MINUS, DSCAN_COLON },
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_WORLD_0, DSCAN_AT }, // dead acute
	{ SDLK_WORLD_1, DSCAN_SEMICOLON }, // ñ
	// No obvious pick for CLEAR - use Home
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_WORLD_2, DSCAN_SLASH }, // ç
};
#endif

/* fi, Finnish, QWERTY */
static struct sym_dkey_mapping keymap_fi[] = {
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_COMPOSE, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_69, DSCAN_AT }, // å
	{ SDLK_WORLD_86, DSCAN_SEMICOLON }, // ö
	{ SDLK_WORLD_7, DSCAN_CLEAR, 1 }, // §
#else
	{ SDLK_WORLD_1, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_3, DSCAN_AT }, // å
	{ SDLK_WORLD_5, DSCAN_SEMICOLON }, // ö
	{ SDLK_WORLD_4, DSCAN_CLEAR, 1 }, // ä
#endif
};

/* fr, French, AZERTY */
static struct sym_dkey_mapping keymap_fr[] = {
	{ SDLK_AMPERSAND, DSCAN_1 },
	{ SDLK_QUOTEDBL, DSCAN_3 },
	{ SDLK_QUOTE, DSCAN_4 },
	{ SDLK_LEFTPAREN, DSCAN_5 },
	{ SDLK_RIGHTPAREN, DSCAN_COLON },
	{ SDLK_a, DSCAN_Q },
	{ SDLK_z, DSCAN_W },
	{ SDLK_CARET, DSCAN_AT },
	{ SDLK_q, DSCAN_A },
	{ SDLK_m, DSCAN_SEMICOLON },
	{ SDLK_w, DSCAN_Z },
	{ SDLK_COMMA, DSCAN_M },
	{ SDLK_SEMICOLON, DSCAN_COMMA },
	{ SDLK_COLON, DSCAN_FULL_STOP },
#ifndef HAVE_COCOA
	{ SDLK_WORLD_73, DSCAN_2 }, // é
	{ SDLK_MINUS, DSCAN_6 },
	{ SDLK_WORLD_72, DSCAN_7 }, // è
	{ SDLK_UNDERSCORE, DSCAN_8 },
	{ SDLK_WORLD_71, DSCAN_9 }, // ç
	{ SDLK_WORLD_64, DSCAN_0 }, // à
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_WORLD_18, DSCAN_CLEAR, 1 }, // ²
	{ SDLK_EXCLAIM, DSCAN_SLASH },
#else
	{ SDLK_WORLD_0, DSCAN_2 }, // é
	{ SDLK_WORLD_1, DSCAN_6 }, // §
	{ SDLK_WORLD_3, DSCAN_7 }, // è
	{ SDLK_EXCLAIM, DSCAN_8 },
	{ SDLK_WORLD_2, DSCAN_9 }, // ç
	{ SDLK_WORLD_4, DSCAN_0 }, // à
	{ SDLK_MINUS, DSCAN_MINUS },
	{ SDLK_BACKQUOTE, DSCAN_CLEAR, 1 },
	{ SDLK_EQUALS, DSCAN_SLASH },
#endif
};

/* fr_CA, Canadian French, QWERTY */
static struct sym_dkey_mapping keymap_fr_CA[] = {
	{ SDLK_MINUS, DSCAN_COLON },
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_CARET, DSCAN_AT },
	{ SDLK_SEMICOLON, DSCAN_SEMICOLON },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
#ifndef HAVE_COCOA
	{ SDLK_COMPOSE, DSCAN_CLEAR, 1 }, // various
	{ SDLK_WORLD_73, DSCAN_SLASH }, // é
#else
	{ SDLK_WORLD_4, DSCAN_CLEAR, 1 }, // ù
	{ SDLK_WORLD_3, DSCAN_SLASH }, // é
#endif
};

/* is, Icelandic, QWERTY */
static struct sym_dkey_mapping keymap_is[] = {
	{ SDLK_MINUS, DSCAN_MINUS },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
#ifndef HAVE_COCOA
	{ SDLK_WORLD_86, DSCAN_COLON }, // ö
	{ SDLK_WORLD_80, DSCAN_AT }, // ð
	{ SDLK_WORLD_70, DSCAN_SEMICOLON }, // æ
	{ SDLK_COMPOSE, DSCAN_CLEAR, 1 }, // dead ring
	{ SDLK_WORLD_94, DSCAN_SLASH }, // þ
#else
	{ SDLK_WORLD_1, DSCAN_COLON }, // ö
	{ SDLK_WORLD_2, DSCAN_AT }, // ð
	{ SDLK_WORLD_4, DSCAN_SEMICOLON }, // æ
	{ SDLK_WORLD_3, DSCAN_CLEAR, 1 }, // dead acute
	{ SDLK_WORLD_5, DSCAN_SLASH }, // þ
#endif
};

/* it, Italian, QWERTY */
static struct sym_dkey_mapping keymap_it[] = {
	{ SDLK_QUOTE, DSCAN_COLON },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_WORLD_76, DSCAN_MINUS }, // ì
	{ SDLK_WORLD_72, DSCAN_AT }, // è
	{ SDLK_WORLD_82, DSCAN_SEMICOLON }, // ò
	{ SDLK_WORLD_89, DSCAN_CLEAR, 1 }, // ù
#else
	{ SDLK_WORLD_0, DSCAN_MINUS }, // ì
	{ SDLK_WORLD_1, DSCAN_AT }, // è
	{ SDLK_WORLD_3, DSCAN_SEMICOLON }, // ò
	{ SDLK_WORLD_4, DSCAN_CLEAR, 1 }, // ù
#endif
};

#ifdef HAVE_COCOA
/* it, Italian, QZERTY (Apple) */
static struct sym_dkey_mapping keymap_it_apple[] = {
	{ SDLK_AMPERSAND, DSCAN_1 },
	{ SDLK_QUOTEDBL, DSCAN_2 },
	{ SDLK_QUOTE, DSCAN_3 },
	{ SDLK_LEFTPAREN, DSCAN_4 },
	{ SDLK_WORLD_1, DSCAN_5 }, // ç
	{ SDLK_WORLD_0, DSCAN_6 }, // è
	{ SDLK_RIGHTPAREN, DSCAN_7 },
	{ SDLK_WORLD_3, DSCAN_8 }, // £
	{ SDLK_WORLD_2, DSCAN_9 }, // à
	{ SDLK_WORLD_4, DSCAN_0 }, // é
	{ SDLK_MINUS, DSCAN_COLON },
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_z, DSCAN_W },
	{ SDLK_WORLD_5, DSCAN_AT }, // ì
	{ SDLK_m, DSCAN_SEMICOLON },
	{ SDLK_WORLD_7, DSCAN_CLEAR, 1 }, // §
	{ SDLK_w, DSCAN_Z },
	{ SDLK_COMMA, DSCAN_M },
	{ SDLK_SEMICOLON, DSCAN_COMMA },
	{ SDLK_COLON, DSCAN_FULL_STOP },
	{ SDLK_WORLD_8, DSCAN_SLASH }, // ò
};
#endif

/* nl, Dutch, QWERTY */
static struct sym_dkey_mapping keymap_nl[] = {
#ifndef HAVE_COCOA
	{ SDLK_SLASH, DSCAN_COLON },
	{ SDLK_WORLD_16, DSCAN_MINUS }, // °
	{ SDLK_COMPOSE, DSCAN_AT }, // dead diaeresis
	{ SDLK_PLUS, DSCAN_SEMICOLON },
	// No obvious pick for CLEAR - use Home
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#else
	{ SDLK_MINUS, DSCAN_COLON },
	{ SDLK_EQUALS, DSCAN_MINUS },
	{ SDLK_LEFTBRACKET, DSCAN_AT },
	{ SDLK_SEMICOLON, DSCAN_SEMICOLON },
	{ SDLK_BACKQUOTE, DSCAN_CLEAR, 1 },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_SLASH, DSCAN_SLASH },
#endif
};

/* no, Norwegian, QWERTY */
static struct sym_dkey_mapping keymap_no[] = {
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#ifndef HAVE_COCOA
	{ SDLK_BACKSLASH, DSCAN_MINUS },
	{ SDLK_WORLD_69, DSCAN_AT }, // å
	{ SDLK_WORLD_88, DSCAN_SEMICOLON }, // ø
	{ SDLK_WORLD_70, DSCAN_CLEAR, 1 }, // æ
	{ SDLK_COMPOSE, DSCAN_CLEAR, 1 }, // dead diaeresis
#else
	{ SDLK_WORLD_0, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_2, DSCAN_AT }, // å
	{ SDLK_WORLD_4, DSCAN_SEMICOLON }, // æ
	{ SDLK_WORLD_3, DSCAN_CLEAR, 1 }, // ø
#endif
};

/* pl, Polish, QWERTZ */
static struct sym_dkey_mapping keymap_pl[] = {
#ifndef HAVE_COCOA
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_QUOTE, DSCAN_MINUS },
	{ SDLK_WORLD_31, DSCAN_AT }, // ż
	{ SDLK_z, DSCAN_Y },
	{ SDLK_WORLD_19, DSCAN_SEMICOLON }, // ł
	{ SDLK_WORLD_95, DSCAN_CLEAR, 1 }, // ˙
	{ SDLK_y, DSCAN_Z },
	{ SDLK_PERIOD, DSCAN_COMMA },
	{ SDLK_COMMA, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#else
	{ SDLK_WORLD_0, DSCAN_COLON }, // ż
	{ SDLK_LEFTBRACKET, DSCAN_MINUS },
	{ SDLK_WORLD_1, DSCAN_AT }, // ó
	{ SDLK_z, DSCAN_Y },
	{ SDLK_WORLD_3, DSCAN_SEMICOLON }, // ł
	{ SDLK_WORLD_2, DSCAN_CLEAR, 1 }, // ą
	{ SDLK_y, DSCAN_Z },
	{ SDLK_PERIOD, DSCAN_COMMA },
	{ SDLK_COMMA, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#endif
};

/* se, Swedish, QWERTY */
static struct sym_dkey_mapping keymap_se[] = {
#ifndef HAVE_COCOA
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_COMPOSE, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_69, DSCAN_AT }, // å
	{ SDLK_WORLD_86, DSCAN_SEMICOLON }, // ö
	{ SDLK_WORLD_7, DSCAN_CLEAR, 1 }, // §
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#else
	{ SDLK_PLUS, DSCAN_COLON },
	{ SDLK_WORLD_1, DSCAN_MINUS }, // dead acute
	{ SDLK_WORLD_3, DSCAN_AT }, // å
	{ SDLK_WORLD_5, DSCAN_SEMICOLON }, // ö
	{ SDLK_WORLD_4, DSCAN_CLEAR, 1 }, // ä
	{ SDLK_COMMA, DSCAN_COMMA },
	{ SDLK_PERIOD, DSCAN_FULL_STOP },
	{ SDLK_MINUS, DSCAN_SLASH },
#endif
};

/* DVORAK */
static struct sym_dkey_mapping keymap_dvorak[] = {
	{ SDLK_LEFTBRACKET, DSCAN_COLON },
	{ SDLK_RIGHTBRACKET, DSCAN_MINUS },
	{ SDLK_QUOTE, DSCAN_Q },
	{ SDLK_COMMA, DSCAN_W },
	{ SDLK_PERIOD, DSCAN_E },
	{ SDLK_p, DSCAN_R },
	{ SDLK_y, DSCAN_T },
	{ SDLK_f, DSCAN_Y },
	{ SDLK_g, DSCAN_U },
	{ SDLK_c, DSCAN_I },
	{ SDLK_r, DSCAN_O },
	{ SDLK_l, DSCAN_P },
	{ SDLK_SLASH, DSCAN_AT },
	{ SDLK_a, DSCAN_A },
	{ SDLK_o, DSCAN_S },
	{ SDLK_e, DSCAN_D },
	{ SDLK_u, DSCAN_F },
	{ SDLK_i, DSCAN_G },
	{ SDLK_d, DSCAN_H },
	{ SDLK_h, DSCAN_J },
	{ SDLK_t, DSCAN_K },
	{ SDLK_n, DSCAN_L },
	{ SDLK_s, DSCAN_SEMICOLON },
	{ SDLK_BACKQUOTE, DSCAN_CLEAR, 1 },
	{ SDLK_SEMICOLON, DSCAN_Z },
	{ SDLK_q, DSCAN_X },
	{ SDLK_j, DSCAN_C },
	{ SDLK_k, DSCAN_V },
	{ SDLK_x, DSCAN_B },
	{ SDLK_b, DSCAN_N },
	{ SDLK_m, DSCAN_M },
	{ SDLK_w, DSCAN_COMMA },
	{ SDLK_v, DSCAN_FULL_STOP },
	{ SDLK_z, DSCAN_SLASH },
};

#define MAPPING(m) .num_mappings = G_N_ELEMENTS(m), .mappings = (m)

static struct keymap keymaps[] = {
	{ .name = "uk", MAPPING(keymap_uk), .description = "UK" },
	{ .name = "cymru", MAPPING(keymap_uk) },
	{ .name = "wales", MAPPING(keymap_uk) },
	{ .name = "eng", MAPPING(keymap_uk) },
	{ .name = "scot", MAPPING(keymap_uk) },

	{ .name = "be", MAPPING(keymap_be), .description = "Belgian" },
	{ .name = "de", MAPPING(keymap_de), .description = "German" },
	{ .name = "dk", MAPPING(keymap_dk), .description = "Danish" },

#ifndef HAVE_COCOA
	{ .name = "es", MAPPING(keymap_es), .description = "Spanish" },
#else
	{ .name = "es", MAPPING(keymap_es_apple), .description = "Spanish" },
	{ .name = "es_iso", MAPPING(keymap_es), .description = "Spanish ISO" },
#endif

	{ .name = "fi", MAPPING(keymap_fi), .description = "Finnish" },
	{ .name = "fr", MAPPING(keymap_fr), .description = "French" },
	{ .name = "fr_CA", MAPPING(keymap_fr_CA), .description = "Canadian French" },
	{ .name = "ie", MAPPING(keymap_uk), .description = "Irish" },
	{ .name = "is", MAPPING(keymap_is), .description = "Icelandic" },

#ifndef HAVE_COCOA
	{ .name = "it", MAPPING(keymap_it), .description = "Italian" },
#else
	{ .name = "it", MAPPING(keymap_it_apple), .description = "Italian" },
	{ .name = "it_pro", MAPPING(keymap_it), .description = "Italian PRO" },
#endif

	{ .name = "nl", MAPPING(keymap_nl), .description = "Dutch" },
	{ .name = "no", MAPPING(keymap_no), .description = "Norwegian" },
	{ .name = "pl", MAPPING(keymap_pl), .description = "Polish QWERTZ" },
	{ .name = "se", MAPPING(keymap_se), .description = "Swedish" },

	{ .name = "us", MAPPING(keymap_uk), .description = "American" },
	{ .name = "dvorak", MAPPING(keymap_dvorak), .description = "DVORAK" },

};
