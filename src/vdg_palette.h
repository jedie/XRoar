/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDG_PALETTE_H_
#define XROAR_VDG_PALETTE_H_

struct vdg_colour_entry {
	float y, chb, b, a;
};

struct vdg_palette {
	const char *name;
	const char *description;
	float sync_y;
	float blank_y;
	float white_y;
	float black_level;
	float rgb_black_level;
	struct vdg_colour_entry palette[12];
};

struct vdg_palette *vdg_palette_new(void);
int vdg_palette_count(void);
struct vdg_palette *vdg_palette_index(int i);
struct vdg_palette *vdg_palette_by_name(const char *name);

/* Map Y'U'V' from palette to pixel value */
void vdg_palette_RGB(struct vdg_palette *vp, _Bool is_pal, int colour,
                     float *Rout, float *Gout, float *Bout);

#endif  /* XROAR_VDG_PALETTE_H_ */
