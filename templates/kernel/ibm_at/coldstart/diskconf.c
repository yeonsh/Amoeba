/*	@(#)diskconf.c	1.6	96/02/26 15:59:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
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
#include "ramdisk.h"
#include "floppy.h"
#include "sd.h"
#include "wini.h"


ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  unittype,
			capacity,     read,     write,   ctrl, geometry */

    { "wini", win_devinit, win_unitinit, win_unittype,
			win_capacity, win_read, win_write, 0, win_geometry },
    { "scsi", scsi_ctlr_init, sd_unit_init, scsi_unit_type,
				sd_capacity, sd_read, sd_write, 0, scsi_geom },
    { "floppy", flp_devinit, flp_unitinit, flp_unittype,
			flp_capacity, flp_read, flp_write, flp_control, 0 },
    { "ramdisk", rd_devinit, rd_unit_init, rd_unit_type,
			rd_capacity, rd_read, rd_write, 0, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] = {
    /*  ctlr_type,	dev_addr,	ivec,	minunit maxunit	*/
    {   0,		0x1F0,		14,	0,	2	},
    {   1,		0x330,		11,	0,	31	},
    {   2,		0x3F2,		6,	0,	2	},
    {	3,		0x000,		0,	0,	1	},
    {   0,		0,		0,	0,	0	}
};

errstat floppy_loader();
static long rdsizes[] = { 4700, 200, 100 };
struct ram_parameters ram_pars[] = {
    {	5001, floppy_loader, "/floppy:00", 0, 3, rdsizes }
};
