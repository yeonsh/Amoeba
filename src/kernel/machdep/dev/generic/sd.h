/*	@(#)sd.h	1.1	96/02/27 13:49:57 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Prototypes for the SCSI disk service functions.
 *
 * Author: Gregory J. Sharp, Jan 1995
 */

#ifndef	__GENERIC_SD_H__
#define	__GENERIC_SD_H__

#include "disk/disk.h"
#include "scsicommon.h"


errstat sd_unit_init _ARGS(( long devaddr, int unit ));
errstat sd_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat sd_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat sd_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));

#endif	/* __GENERIC_SD_H__ */
