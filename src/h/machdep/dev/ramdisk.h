/*	@(#)ramdisk.h	1.3	96/02/27 10:30:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__RAMDISK_H__
#define	__RAMDISK_H__

/* Ram disk's sector definitions */
#define RAM_SECTSHIFT	9			/* log2(512) */
#define RAM_SECTSIZE	(1 << RAM_SECTSHIFT)	/* 512 */

struct ram_parameters {
	int	ramp_nblocks;		/* ram disk's size in blocks */
	errstat	(*ramp_init)();		/* ram disk's init routine */
	char 	*ramp_charstarpar;	/* device name */
	long	ramp_longpar;		/* extra parameter */
	int	ramp_nvdisks;		/* # of vdisks on ram disk */
	long	*ramp_vdisksizes;	/* sizes of vdisks */
};

struct ram_info {
	char	*rami_base;
};


/*
 * Prototypes for the RAMDISK disk service functions.
 */

#include "disk/disk.h"


errstat rd_devinit _ARGS(( long devaddr, int unit ));
int	rd_unit_type _ARGS(( long devaddr, int unit ));
errstat rd_unit_init _ARGS(( long devaddr, int unit ));
errstat rd_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat rd_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat rd_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));

#endif	/* __RAMDISK_H__ */
