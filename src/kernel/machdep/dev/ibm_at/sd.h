/*	@(#)sd.h	1.1	96/02/27 13:52:19 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IBM_AT_SD_H__
#define	__IBM_AT_SD_H__

#include "../generic/sd.h"
#include "disk/disk.h"
#include "vdisk/disklabel.h"

errstat scsi_geom _ARGS(( long devaddr, int unit, geometry * geom ));

#endif /* __IBM_AT_SD_H__ */
