/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDISK_H_
#define XROAR_VDISK_H_

#include <stdint.h>

#define VDISK_LENGTH_5_25 (0x1900)
#define VDISK_LENGTH_8    (0x2940)

#define VDISK_DOUBLE_DENSITY (0x8000)
#define VDISK_SINGLE_DENSITY (0x0000)

struct vdisk {
	int filetype;
	char *filename;
	_Bool file_write_protect;
	_Bool write_protect;
	unsigned num_sides;
	unsigned num_tracks;
	unsigned track_length;
	uint8_t **side_data;
};

struct vdisk *vdisk_blank_disk(unsigned num_sides, unsigned num_tracks,
		unsigned track_length);
void vdisk_destroy(struct vdisk *disk);

struct vdisk *vdisk_load(const char *filename);
_Bool vdisk_save(struct vdisk *disk, _Bool force);

void *vdisk_track_base(struct vdisk *disk, unsigned side, unsigned track);
void *vdisk_extend_disk(struct vdisk *disk, unsigned side, unsigned track);
_Bool vdisk_format_disk(struct vdisk *disk, unsigned density,
		unsigned num_sectors, unsigned first_sector, unsigned ssize_code);
_Bool vdisk_update_sector(struct vdisk *disk, unsigned side, unsigned track,
		unsigned sector, unsigned sector_length, uint8_t *buf);
_Bool vdisk_fetch_sector(struct vdisk *disk, unsigned side, unsigned track,
		unsigned sector, unsigned sector_length, uint8_t *buf);

#endif  /* XROAR_VDISK_H_ */
