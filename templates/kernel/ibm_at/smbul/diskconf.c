/*	@(#)diskconf.c	1.1	96/02/16 15:45:40 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	DISK TABLE
**
**	This file contains the disk configuration information for the disks
**	and disk controllers attached to the machine.  At present it is
**	rather complicated but some day we will try to generate it from a
**	table.
*/

#include "amoeba.h"
#include "disk/conf.h"
#include "floppy.h"
#include "sd.h"
#include "wini.h"


ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  unittype,
				capacity, read, write, ctrl, geometry */

    { "wini", win_devinit, win_unitinit, win_unittype,
			win_capacity, win_read, win_write, 0, win_geometry },
    { "scsi", scsi_ctlr_init, sd_unit_init, scsi_unit_type,
			sd_capacity, sd_read, sd_write, 0, scsi_geom },
    { "floppy", flp_devinit, flp_unitinit, flp_unittype,
			flp_capacity, flp_read, flp_write, flp_control, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] =
{
    /*  ctlr_type,	dev_addr,	ivec,	minunit maxunit	*/

    {   0,		0x1F0,		14,	0,	2	},
    {   1,		0x330,		11,	0,	31      },
    {   2,		0x3F2,		6,	0,	2	},
    {   0,		0,		0,	0,	0	}
};
