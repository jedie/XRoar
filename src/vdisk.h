/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_VDISK_H_
#define XROAR_VDISK_H_

#include <stdint.h>

#define VDISK_LENGTH_5_25 (0x1900)
#define VDISK_LENGTH_8    (0x2940)

#define VDISK_DOUBLE_DENSITY (0x8000)
#define VDISK_SINGLE_DENSITY (0x0000)

/*
 * If write_back is not set, the image file will not be updated when the disk
 * is ejected or flushed.
 *
 * If write_protect is set, the FDC will not be permitted to write to the disk
 * image.
 *
 * The actual track data is organised by disk side - this makes dynamically
 * expanding disks easier.
 */

struct vdisk {
	int filetype;
	char *filename;
	_Bool write_back;
	_Bool write_protect;
	unsigned num_cylinders;
	unsigned num_heads;
	unsigned track_length;
	uint8_t **side_data;
};

struct vdisk *vdisk_blank_disk(unsigned ncyls, unsigned nheads,
			       unsigned track_length);
void vdisk_destroy(struct vdisk *disk);

struct vdisk *vdisk_load(const char *filename);
int vdisk_save(struct vdisk *disk, _Bool force);

/*
 * These both return a pointer to the beginning of the specified track's IDAM
 * list (followed by the track data).  vdisk_extend_disk() is called before
 * writing, as it additionally extends the disk if cyl or head exceed the
 * currently configured values.  NULL return indicates error.
 */

void *vdisk_track_base(struct vdisk *disk, unsigned cyl, unsigned head);
void *vdisk_extend_disk(struct vdisk *disk, unsigned cyl, unsigned head);

int vdisk_format_track(struct vdisk *disk, _Bool double_density,
		       unsigned cyl, unsigned head,
		       unsigned nsectors, unsigned first_sector, unsigned ssize_code);
int vdisk_format_disk(struct vdisk *disk, _Bool double_density,
		      unsigned nsectors, unsigned first_sector, unsigned ssize_code);
int vdisk_update_sector(struct vdisk *disk, unsigned cyl, unsigned head,
			unsigned sector, unsigned sector_length, uint8_t *buf);
int vdisk_fetch_sector(struct vdisk *disk, unsigned cyl, unsigned head,
		       unsigned sector, unsigned sector_length, uint8_t *buf);

#endif  /* XROAR_VDISK_H_ */
