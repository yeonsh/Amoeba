/*	@(#)vollabel.h	1.2	94/04/06 15:47:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Definitions for multi volume dumps
 */

/*
 * Volume header.
 * In the actual dump all the values appear in little endian,
 * the last volume has a nextvol value of -1.
 */
struct volume {
    long vol_magic;			/* magic cookie */
    long vol_mediasize;			/* media size (in blocks) */
    long vol_datasize;			/* size of data on media (in blocks) */
    long vol_curvol;			/* current volume */
    long vol_nextvol;			/* next volume */
    unsigned short vol_crc;		/* crc check sum */
    char vol_name[20];			/* volume name */
};

/*
 * Volume header as it appears on disk
 */
union vollabel {
    struct volume vl_volume;
    char vl_pad[512];
};

/* magic volume number and default name */
#define	VL_MAGIC	0xABCD1234
#define	VL_NAME		"AMOEBA"

/* less typing */
#define	vl_magic	vl_volume.vol_magic
#define	vl_mediasize	vl_volume.vol_mediasize
#define	vl_datasize	vl_volume.vol_datasize
#define	vl_curvol	vl_volume.vol_curvol
#define	vl_nextvol	vl_volume.vol_nextvol
#define	vl_crc		vl_volume.vol_crc
#define	vl_name		vl_volume.vol_name

