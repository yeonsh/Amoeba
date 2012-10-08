/*	@(#)disk.h	1.1	96/02/27 10:36:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DISK_DISK_H__
#define __DISK_DISK_H__

/*
**	disk.h
**
**	Constants and types related to the disk server.
*/

typedef	int32	disk_addr;	/* disk address */

/*
** decode/encode an disk address from/to host architecture to/from big-endian
** format.  DEC_L_BE and ENC_L_BE are defined in byteorder.h
*/

#define	DEC_DISK_ADDR_BE(x)	DEC_INT32_BE(x)
#define	ENC_DISK_ADDR_BE(x)	ENC_INT32_BE(x)
#define	DEC_DISK_ADDR_LE(x)	DEC_INT32_LE(x)
#define	ENC_DISK_ADDR_LE(x)	ENC_INT32_LE(x)
#define	buf_put_disk_addr	buf_put_int32
#define	buf_get_disk_addr	buf_get_int32

/* Physical disk block size. */
#define	D_PHYS_SHIFT		9	/* == log2(D_PHYSBLKSZ) */
#define	D_PHYSBLKSZ		(1 << D_PHYS_SHIFT)

/* size of a unit of info returned by the DS_INFO command */
#define DISKINFOSZ              (sizeof (int32) + 2*sizeof (disk_addr))

typedef struct dk_info_data dk_info_data;

struct dk_info_data
{
    int32	d_unit;
    disk_addr	d_firstblk;
    disk_addr	d_numblks;
};


/* size of disk thread's transaction buffer */
#define	D_REQBUFSZ		(29 * 1024)

/* Disk server commands */
#define	DS_INFO			(DISK_FIRST_COM + 1)
#define	DS_READ			(DISK_FIRST_COM + 2)
#define	DS_SIZE			(DISK_FIRST_COM + 3)
#define	DS_WRITE		(DISK_FIRST_COM + 4)
#define DS_GETGEOMETRY		(DISK_FIRST_COM + 5)
#define	DS_CONTROL		(DISK_FIRST_COM + 6)

/* Subcommands for DS_CONTROL */
#define	DSC_EJECT		1	/* for floppies and CD-ROMs */
#define	DSC_FORMAT		2	/* for floppies */

/* Rights for Disk capabilities */
#define	RGT_READ		0x02
#define	RGT_WRITE		0x04
#define	RGT_CTL			0x08

#endif /* __DISK_DISK_H__ */
