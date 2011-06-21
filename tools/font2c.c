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

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>

const char *argv0;
const char *array_name = "font";
const char *array_type = "unsigned int";
enum { NORMAL, VDG } output_mode = NORMAL;

Uint32 getpixel(SDL_Surface *surface, int x, int y);
void print_usage(FILE *f);

#ifdef main
#undef main
#endif

int main(int argc, char **argv) {
	SDL_Surface *in;
	int i, num_chars, char_height;
	int header_only = 0;

	argv0 = argv[0];
	for (i = 1; i < argc; i++) {
		if (0 == strcmp(argv[i], "--")) {
			i++;
			break;
		}
		if (argv[i][0] != '-')
			break;
		if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "--help")) {
			print_usage(stdout);
			exit(0);
		}
		if (0 == strcmp(argv[i], "--vdg")) {
			output_mode = VDG;
		} else if (0 == strcmp(argv[i], "--array") && i+1 < argc) {
			array_name = argv[++i];
		} else if (0 == strcmp(argv[i], "--type") && i+1 < argc) {
			array_type = argv[++i];
		} else if (0 == strcmp(argv[i], "--header")) {
			header_only = 1;
		} else {
			fprintf(stderr, "%s: unrecognised option '%s'\nTry '%s --help' for more information.\n", argv0, argv[i], argv0);
			exit(1);
		}
	}

	if (i >= argc) {
		print_usage(stderr);
		exit(1);
	}

	in = IMG_Load(argv[i]);
	if (in == NULL) {
		fprintf(stderr, "%s: %s: %s\n", argv0, argv[i], IMG_GetError());
		exit(1);
	}
	if (in->w != 128 || in->h != 48) {
		fprintf(stderr, "%s: %s: Wrong resolution for a font image file\n", argv0, argv[i]);
		exit(1);
	}

	if (output_mode == VDG) {
		num_chars = 64;
		char_height = 12;
	} else {
		num_chars = 96;
		char_height = 8;
	}

	printf("/* This file was automatically generated\n * by %s from %s */\n\n", argv0, argv[i]);
	if (header_only) {
		printf("extern const %s %s[%d];\n", array_type, array_name, num_chars * char_height);
		return 0;
	}
	printf("const %s %s[%d] = {\n", array_type, array_name, num_chars * char_height);

	for (i = 0; i < num_chars; i++) {
		int c, xbase, ybase, j;
		if (output_mode == VDG)
			c = (i & 0x3f) ^ 0x20;
		else
			c = i;
		xbase = (c & 15) * 8;
		ybase = (c >> 4) * 8;
		printf("\t");
		if (output_mode == VDG)
			printf("0x00, 0x00, 0x00, ");
		for (j = 0; j < 8; j++) {
			int b = 0;
			int k;
			if (j > 0) printf(", ");
			for (k = 0; k < 8; k++) {
				b <<= 1;
				b |= (getpixel(in, xbase + k, ybase + j) == 0) ? 0 : 1;
			}
			printf("0x%02x", b);
		}
		if (output_mode == VDG)
			printf(", 0x00");
		if (i < (num_chars - 1)) printf(",");
		printf("\n");
	}
	printf("};\n");

	return 0;
}

Uint32 getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch(bpp) {
		case 1: return *p;
		case 2: return *(Uint16 *)p;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
		case 4: return *(Uint32 *)p;
		default: return 0;
	}
}

void print_usage(FILE *f) {
	fprintf(f, "Usage: %s [OPTION]... font-image-file\n\n", argv0);
	fprintf(f, "      --array                name of array to use in generated C code [font]\n");
	fprintf(f, "      --type                 data type for generated array [unsigned int]\n");
	fprintf(f, "      --vdg                  pad out font to 12-lines for VDG code\n");
	fprintf(f, "  -h, --help     display this help and exit\n");
}
