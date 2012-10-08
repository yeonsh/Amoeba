/*	@(#)disk.h	1.4	96/02/27 10:32:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_DISK_H__
#define __MODULE_DISK_H__

#include "_ARGS.h"
#include "server/disk/disk.h"
#include "server/vdisk/disklabel.h"

#define	disk_control		_disk_control
#define	disk_info		_disk_info
#define	disk_read		_disk_read
#define	disk_size		_disk_size
#define	disk_write		_disk_write
#define	disk_getgeometry	_disk_getgeometry

errstat	disk_control _ARGS(( capability *, int, bufptr, bufsize ));
errstat	disk_info _ARGS((capability *, dk_info_data *, int, int *));
errstat	disk_read _ARGS((capability *, int, disk_addr, disk_addr, bufptr));
errstat	disk_size _ARGS((capability *, int, disk_addr *));
errstat	disk_write _ARGS((capability *, int, disk_addr, disk_addr, bufptr));
errstat disk_getgeometry _ARGS(( capability *, geometry * ));

#endif /* __MODULE_DISK_H__ */
