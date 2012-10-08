/*	@(#)diskconf.c	1.7	96/02/26 15:59:58 */
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
#include "ramdisk.h"
#include "floppy.h"

ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  unittype,
				capacity,  read,  write, ctrl, geometry */

    { "floppy", flp_devinit, flp_unitinit, flp_unittype,
			flp_capacity, flp_read, flp_write, flp_control, 0 },
    { "ramdisk", rd_devinit, rd_unit_init, rd_unit_type,
			rd_capacity, rd_read, rd_write, 0, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] = {
    /*  ctlr_type,	dev_addr,	ivec,	minunit maxunit	*/
    {   0,		0x3F2,		6,	0,	2	},
    {	1,		0x000,		0,	0,	1	},
    {   0,		0,		0,	0,	0	}
};

static long rdsizes[] = { 4700, 200, 100 };
struct ram_parameters ram_pars[] = {
    {	5001, 0, "/floppy:00", 0, 3, rdsizes }
};
