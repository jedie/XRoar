/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2013  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDISK_H_
#define XROAR_VDISK_H_

#include <inttypes.h>

#define VDISK_LENGTH_5_25 (0x1900)
#define VDISK_LENGTH_8    (0x2940)

#define VDISK_DOUBLE_DENSITY (0x8000)
#define VDISK_SINGLE_DENSITY (0x0000)

struct vdisk {
	int filetype;
	char *filename;
	_Bool file_write_protect;
	_Bool write_protect;
	unsigned int num_sides;
	unsigned int num_tracks;
	unsigned int track_length;
	uint8_t *track_data;
};

struct vdisk *vdisk_blank_disk(int num_sides, int num_tracks,
		int track_length);
void vdisk_destroy(struct vdisk *disk);

struct vdisk *vdisk_load(const char *filename);
int vdisk_save(struct vdisk *disk, int force);

void *vdisk_track_base(struct vdisk *disk, int side, int track);
int vdisk_format_disk(struct vdisk *disk, int density,
		int num_sectors, int first_sector, int ssize_code);
int vdisk_update_sector(struct vdisk *disk, int side, int track,
		int sector, int sector_length, uint8_t *buf);
int vdisk_fetch_sector(struct vdisk *disk, int side, int track,
		int sector, int sector_length, uint8_t *buf);

#endif  /* XROAR_VDISK_H_ */
