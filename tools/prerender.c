#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../vdg_bitmaps.c"

#define MAPCOLOUR(r,g,b) ((r & 0xc0) | (g & 0xe0) >> 2 | (b & 0xe0) >> 5)

unsigned int bitmaps[4][8 * 12 * 256];

uint8_t vdg_colour[8];
uint8_t fg, fg2, bg, black;

#ifdef main
#undef main
#endif

int main(int argc, char **argv) {
	int i,j,k,l;
	const unsigned int *s = vdg_alpha;
	unsigned int c;

	memset(bitmaps, 0, sizeof(bitmaps));

	vdg_colour[0] = MAPCOLOUR(0x00, 0xff, 0x00);
	vdg_colour[1] = MAPCOLOUR(0xff, 0xff, 0x00);
	vdg_colour[2] = MAPCOLOUR(0x00, 0x00, 0xff);
	vdg_colour[3] = MAPCOLOUR(0xff, 0x00, 0x00);
	vdg_colour[4] = MAPCOLOUR(0xff, 0xe0, 0xe0);
	vdg_colour[5] = MAPCOLOUR(0x00, 0xff, 0xff);
	vdg_colour[6] = MAPCOLOUR(0xff, 0x00, 0xff);
	vdg_colour[7] = MAPCOLOUR(0xff, 0xa5, 0x00);
	fg = vdg_colour[0];
	fg2 = vdg_colour[7];
	bg = MAPCOLOUR(0x00, 0x20, 0x00);
	black = MAPCOLOUR(0x00, 0x00, 0x00);

	/* Pre-render alphanumeric characters */
	for (l = 0; l < 128; l++) {
		if (l == 64)
			s = vdg_alpha;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++) {
				c = *(s++);
				if (l & 0x40)
					c = ~c;
				for (k = 0; k < 8; k++) {
					bitmaps[0][i*8192 + l*32 + j*8 + k] = (c & (1<<(7-k))) ? fg : bg;
					bitmaps[1][i*8192 + l*32 + j*8 + k] = (c & (1<<(7-k))) ? fg2 : bg;
				}
			}
		}
	}
	/* Pre-render "external" charset characters */
	for (l = 0; l < 128; l++) {
		c = l;
		if (c & 0x40)
			c = ~c;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++) {
				for (k = 0; k < 8; k++) {
					bitmaps[2][i*8192 + l*32 + j*8 + k] = (c & (1<<(7-k))) ? fg : bg;
					bitmaps[3][i*8192 + l*32 + j*8 + k] = (c & (1<<(7-k))) ? fg2 : bg;
				}
			}
		}
	}
	/* Pre-render semigraphics 4 characters */
	for (l = 128; l < 256; l++) {
		uint8_t tmp = vdg_colour[(l & 0x70)>>4];
		for (k = 0; k < 4; k++) {
			bitmaps[0][0*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 2*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 3*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 2*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 3*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 0*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 1*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 0*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 1*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp : black;
		}
		for (k = 4; k < 8; k++) {
			bitmaps[0][0*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 2*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][0*8192 + l*32 + 3*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 2*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][0*8192 + l*32 + 3*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[0][1*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 0*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 1*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[0][2*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][1*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 0*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 1*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[1][2*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp : black;
		}
	}
	/* Pre-render semigraphics 6 characters */
	for (l = 128; l < 256; l++) {
		uint8_t tmp = vdg_colour[(l & 0xc0)>>6];
		uint8_t tmp2 = vdg_colour[4+((l & 0xc0)>>6)];
		for (k = 0; k < 4; k++) {
			bitmaps[2][0*8192 + l*32 + 0*8 + k] = (l & 0x20) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 1*8 + k] = (l & 0x20) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 2*8 + k] = (l & 0x20) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 3*8 + k] = (l & 0x20) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 2*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 3*8 + k] = (l & 0x08) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 0*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 1*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp : black;
			bitmaps[3][0*8192 + l*32 + 0*8 + k] = (l & 0x20) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 1*8 + k] = (l & 0x20) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 2*8 + k] = (l & 0x20) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 3*8 + k] = (l & 0x20) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 0*8 + k] = (l & 0x08) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 1*8 + k] = (l & 0x08) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 2*8 + k] = (l & 0x08) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 3*8 + k] = (l & 0x08) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 0*8 + k] = (l & 0x02) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 1*8 + k] = (l & 0x02) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 2*8 + k] = (l & 0x02) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 3*8 + k] = (l & 0x02) ? tmp2 : black;
		}
		for (k = 4; k < 8; k++) {
			bitmaps[2][0*8192 + l*32 + 0*8 + k] = (l & 0x10) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 1*8 + k] = (l & 0x10) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 2*8 + k] = (l & 0x10) ? tmp : black;
			bitmaps[2][0*8192 + l*32 + 3*8 + k] = (l & 0x10) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 2*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[2][1*8192 + l*32 + 3*8 + k] = (l & 0x04) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 0*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 1*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[2][2*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp : black;
			bitmaps[3][0*8192 + l*32 + 0*8 + k] = (l & 0x10) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 1*8 + k] = (l & 0x10) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 2*8 + k] = (l & 0x10) ? tmp2 : black;
			bitmaps[3][0*8192 + l*32 + 3*8 + k] = (l & 0x10) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 0*8 + k] = (l & 0x04) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 1*8 + k] = (l & 0x04) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 2*8 + k] = (l & 0x04) ? tmp2 : black;
			bitmaps[3][1*8192 + l*32 + 3*8 + k] = (l & 0x04) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 0*8 + k] = (l & 0x01) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 1*8 + k] = (l & 0x01) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 2*8 + k] = (l & 0x01) ? tmp2 : black;
			bitmaps[3][2*8192 + l*32 + 3*8 + k] = (l & 0x01) ? tmp2 : black;
		}
	}
	puts("/* This is a generated file. */");
	puts("#include \"types.h\"");
	puts("const uint8_t vdg_alpha_gp32[4][3][8192] = {\n");
	for (l = 0; l < 4; l++) {
		printf("\t{\n");
		for (i = 0; i < 3; i++) {
		printf("\t\t{\n");
			for (j = 0; j < 256; j++) {
				printf("\t\t\t");
				for (k = 0; k < 32; k++) {
					printf("0x%02x, ", bitmaps[l][i*8192+j*32+k]);
				}
				printf("\n");
			}
			printf("\t\t},\n");
		}
		printf("\t},\n");
	}
	printf("};\n");
	return 0;
}
