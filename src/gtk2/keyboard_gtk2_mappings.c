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

/* Keymaps map GDK keysym to dkey. */

/* uk, United Kingdom, QWERTY */
/* cymru, Welsh, QWERTY */
/* eng, English, QWERTY */
/* scot, Scottish, QWERTY */
/* ie, Irish, QWERTY */
/* usa, USA, QWERTY */
static struct sym_dkey_mapping keymap_uk[] = {
	{ GDK_KEY_minus, DSCAN_COLON },
	{ GDK_KEY_equal, DSCAN_MINUS },
	{ GDK_KEY_bracketleft, DSCAN_AT },
	{ GDK_KEY_semicolon, DSCAN_SEMICOLON },
	{ GDK_KEY_grave, DSCAN_CLEAR, 1 },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_slash, DSCAN_SLASH },
};

/* be, Belgian, AZERTY */
static struct sym_dkey_mapping keymap_be[] = {
	{ GDK_KEY_ampersand, DSCAN_1 },
	{ GDK_KEY_eacute, DSCAN_2 },
	{ GDK_KEY_quotedbl, DSCAN_3 },
	{ GDK_KEY_apostrophe, DSCAN_4 },
	{ GDK_KEY_parenleft, DSCAN_5 },
	{ GDK_KEY_section, DSCAN_6 },
	{ GDK_KEY_egrave, DSCAN_7 },
	{ GDK_KEY_exclam, DSCAN_8 },
	{ GDK_KEY_ccedilla, DSCAN_9 },
	{ GDK_KEY_agrave, DSCAN_0 },
	{ GDK_KEY_parenright, DSCAN_COLON },
	{ GDK_KEY_minus, DSCAN_MINUS },
	{ GDK_KEY_a, DSCAN_Q },
	{ GDK_KEY_z, DSCAN_W },
	{ GDK_KEY_dead_circumflex, DSCAN_AT },
	{ GDK_KEY_q, DSCAN_A },
	{ GDK_KEY_m, DSCAN_SEMICOLON },
	{ GDK_KEY_twosuperior, DSCAN_CLEAR, 1 },
	{ GDK_KEY_w, DSCAN_Z },
	{ GDK_KEY_comma, DSCAN_M },
	{ GDK_KEY_semicolon, DSCAN_COMMA },
	{ GDK_KEY_colon, DSCAN_FULL_STOP },
	{ GDK_KEY_equal, DSCAN_SLASH },
};

/* de, German, QWERTZ */
static struct sym_dkey_mapping keymap_de[] = {
	{ GDK_KEY_ssharp, DSCAN_COLON },
	{ GDK_KEY_dead_acute, DSCAN_MINUS },
	{ GDK_KEY_z, DSCAN_Y },
	{ GDK_KEY_udiaeresis, DSCAN_AT },
	{ GDK_KEY_odiaeresis, DSCAN_SEMICOLON },
	{ GDK_KEY_dead_circumflex, DSCAN_CLEAR, 1 },
	{ GDK_KEY_y, DSCAN_Z },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* dk, Danish, QWERTY */
static struct sym_dkey_mapping keymap_dk[] = {
	{ GDK_KEY_plus, DSCAN_COLON },
	{ GDK_KEY_dead_acute, DSCAN_MINUS }, // dead acute
	{ GDK_KEY_aring, DSCAN_AT }, // å
	{ GDK_KEY_ae, DSCAN_SEMICOLON }, // æ
	{ GDK_KEY_onehalf, DSCAN_CLEAR, 1 }, // ½
	{ GDK_KEY_oslash, DSCAN_CLEAR, 1 }, // ø
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* es, Spanish, QWERTY */
static struct sym_dkey_mapping keymap_es[] = {
	{ GDK_KEY_apostrophe, DSCAN_COLON },
	{ GDK_KEY_exclamdown, DSCAN_MINUS },
	{ GDK_KEY_dead_grave, DSCAN_AT },
	{ GDK_KEY_ntilde, DSCAN_SEMICOLON },
	{ GDK_KEY_masculine, DSCAN_CLEAR, 1 },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* fi, Finnish, QWERTY */
static struct sym_dkey_mapping keymap_fi[] = {
	{ GDK_KEY_plus, DSCAN_COLON },
	{ GDK_KEY_dead_acute, DSCAN_MINUS }, // dead acute
	{ GDK_KEY_aring, DSCAN_AT }, // å
	{ GDK_KEY_odiaeresis, DSCAN_SEMICOLON }, // ö
	{ GDK_KEY_section, DSCAN_CLEAR, 1 }, // §
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* fr, French, AZERTY */
static struct sym_dkey_mapping keymap_fr[] = {
	{ GDK_KEY_ampersand, DSCAN_1 },
	{ GDK_KEY_eacute, DSCAN_2 },
	{ GDK_KEY_quotedbl, DSCAN_3 },
	{ GDK_KEY_apostrophe, DSCAN_4 },
	{ GDK_KEY_parenleft, DSCAN_5 },
	{ GDK_KEY_minus, DSCAN_6 },
	{ GDK_KEY_egrave, DSCAN_7 },
	{ GDK_KEY_underscore, DSCAN_8 },
	{ GDK_KEY_ccedilla, DSCAN_9 },
	{ GDK_KEY_agrave, DSCAN_0 },
	{ GDK_KEY_parenright, DSCAN_COLON },
	{ GDK_KEY_equal, DSCAN_MINUS },
	{ GDK_KEY_a, DSCAN_Q },
	{ GDK_KEY_z, DSCAN_W },
	{ GDK_KEY_dead_circumflex, DSCAN_AT },
	{ GDK_KEY_q, DSCAN_A },
	{ GDK_KEY_m, DSCAN_SEMICOLON },
	{ GDK_KEY_twosuperior, DSCAN_CLEAR, 1 },
	{ GDK_KEY_w, DSCAN_Z },
	{ GDK_KEY_comma, DSCAN_M },
	{ GDK_KEY_semicolon, DSCAN_COMMA },
	{ GDK_KEY_colon, DSCAN_FULL_STOP },
	{ GDK_KEY_exclam, DSCAN_SLASH },
};

/* fr_CA, Canadian French, QWERTY */
static struct sym_dkey_mapping keymap_fr_CA[] = {
	{ GDK_KEY_minus, DSCAN_COLON },
	{ GDK_KEY_equal, DSCAN_MINUS },
	{ GDK_KEY_dead_circumflex, DSCAN_AT },
	{ GDK_KEY_semicolon, DSCAN_SEMICOLON },
	{ GDK_KEY_dead_grave, DSCAN_CLEAR },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_eacute, DSCAN_SLASH },
};

/* is, Icelandic, QWERTY */
static struct sym_dkey_mapping keymap_is[] = {
	{ GDK_KEY_odiaeresis, DSCAN_COLON }, // ö
	{ GDK_KEY_minus, DSCAN_MINUS },
	{ GDK_KEY_eth, DSCAN_AT }, // ð
	{ GDK_KEY_ae, DSCAN_SEMICOLON }, // æ
	{ GDK_KEY_dead_abovering, DSCAN_CLEAR, 1 }, // dead ring
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_thorn, DSCAN_SLASH }, // þ
};

/* it, Italian, QWERTY */
static struct sym_dkey_mapping keymap_it[] = {
	{ GDK_KEY_apostrophe, DSCAN_COLON },
	{ GDK_KEY_igrave, DSCAN_MINUS }, // ì
	{ GDK_KEY_egrave, DSCAN_AT }, // è
	{ GDK_KEY_ograve, DSCAN_SEMICOLON }, // ò
	{ GDK_KEY_ugrave, DSCAN_CLEAR, 1 }, // ù
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* nl, Dutch, QWERTY */
static struct sym_dkey_mapping keymap_nl[] = {
	{ GDK_KEY_slash, DSCAN_COLON },
	{ GDK_KEY_degree, DSCAN_MINUS },
	{ GDK_KEY_dead_diaeresis, DSCAN_AT },
	{ GDK_KEY_plus, DSCAN_SEMICOLON },
	{ GDK_KEY_dead_acute, DSCAN_CLEAR, 1 },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* no, Norwegian, QWERTY */
static struct sym_dkey_mapping keymap_no[] = {
	{ GDK_KEY_plus, DSCAN_COLON },
	{ GDK_KEY_backslash, DSCAN_MINUS },
	{ GDK_KEY_aring, DSCAN_AT }, // å
	{ GDK_KEY_oslash, DSCAN_SEMICOLON }, // ø
	{ GDK_KEY_ae, DSCAN_CLEAR, 1 }, // æ
	{ GDK_KEY_bar, DSCAN_CLEAR, 1 },
	{ GDK_KEY_dead_diaeresis, DSCAN_CLEAR, 1 }, // dead diaeresis
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* pl, Polish, QWERTZ */
static struct sym_dkey_mapping keymap_pl[] = {
	{ GDK_KEY_plus, DSCAN_COLON },
	{ GDK_KEY_apostrophe, DSCAN_MINUS },
	{ GDK_KEY_z, DSCAN_Y },
	{ GDK_KEY_zabovedot, DSCAN_AT },
	{ GDK_KEY_lstroke, DSCAN_SEMICOLON },
	{ GDK_KEY_abovedot, DSCAN_CLEAR, 1 },
	{ GDK_KEY_y, DSCAN_Z },
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* se, Swedish, QWERTY */
static struct sym_dkey_mapping keymap_se[] = {
	{ GDK_KEY_plus, DSCAN_COLON },
	{ GDK_KEY_dead_acute, DSCAN_MINUS }, // dead acute
	{ GDK_KEY_aring, DSCAN_AT }, // å
	{ GDK_KEY_odiaeresis, DSCAN_SEMICOLON }, // ö
	{ GDK_KEY_section, DSCAN_CLEAR, 1 }, // §
	{ GDK_KEY_adiaeresis, DSCAN_CLEAR, 1 }, // ä
	{ GDK_KEY_comma, DSCAN_COMMA },
	{ GDK_KEY_period, DSCAN_FULL_STOP },
	{ GDK_KEY_minus, DSCAN_SLASH },
};

/* DVORAK */
static struct sym_dkey_mapping keymap_dvorak[] = {
	{ GDK_KEY_bracketleft, DSCAN_COLON },
	{ GDK_KEY_bracketright, DSCAN_MINUS },
	{ GDK_KEY_apostrophe, DSCAN_Q },
	{ GDK_KEY_comma, DSCAN_W },
	{ GDK_KEY_period, DSCAN_E },
	{ GDK_KEY_p, DSCAN_R },
	{ GDK_KEY_y, DSCAN_T },
	{ GDK_KEY_f, DSCAN_Y },
	{ GDK_KEY_g, DSCAN_U },
	{ GDK_KEY_c, DSCAN_I },
	{ GDK_KEY_r, DSCAN_O },
	{ GDK_KEY_l, DSCAN_P },
	{ GDK_KEY_slash, DSCAN_AT },
	{ GDK_KEY_a, DSCAN_A },
	{ GDK_KEY_o, DSCAN_S },
	{ GDK_KEY_e, DSCAN_D },
	{ GDK_KEY_u, DSCAN_F },
	{ GDK_KEY_i, DSCAN_G },
	{ GDK_KEY_d, DSCAN_H },
	{ GDK_KEY_h, DSCAN_J },
	{ GDK_KEY_t, DSCAN_K },
	{ GDK_KEY_n, DSCAN_L },
	{ GDK_KEY_s, DSCAN_SEMICOLON },
	{ GDK_KEY_grave, DSCAN_CLEAR, 1 },
	{ GDK_KEY_semicolon, DSCAN_Z },
	{ GDK_KEY_q, DSCAN_X },
	{ GDK_KEY_j, DSCAN_C },
	{ GDK_KEY_k, DSCAN_V },
	{ GDK_KEY_x, DSCAN_B },
	{ GDK_KEY_b, DSCAN_N },
	{ GDK_KEY_m, DSCAN_M },
	{ GDK_KEY_w, DSCAN_COMMA },
	{ GDK_KEY_v, DSCAN_FULL_STOP },
	{ GDK_KEY_z, DSCAN_SLASH },
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
	{ .name = "es", MAPPING(keymap_es), .description = "Spanish" },
	{ .name = "fi", MAPPING(keymap_fi), .description = "Finnish" },
	{ .name = "fr", MAPPING(keymap_fr), .description = "French" },
	{ .name = "fr_CA", MAPPING(keymap_fr_CA), .description = "Canadian French" },
	{ .name = "ie", MAPPING(keymap_uk), .description = "Irish" },
	{ .name = "is", MAPPING(keymap_is), .description = "Icelandic" },
	{ .name = "it", MAPPING(keymap_it), .description = "Italian" },
	{ .name = "nl", MAPPING(keymap_nl), .description = "Dutch" },
	{ .name = "no", MAPPING(keymap_no), .description = "Norwegian" },
	{ .name = "pl", MAPPING(keymap_pl), .description = "Polish QWERTZ" },
	{ .name = "se", MAPPING(keymap_se), .description = "Swedish" },

	{ .name = "us", MAPPING(keymap_uk), .description = "American" },
	{ .name = "dvorak", MAPPING(keymap_dvorak), .description = "DVORAK" },
};
