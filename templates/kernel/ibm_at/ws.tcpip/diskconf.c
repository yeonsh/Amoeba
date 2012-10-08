/*	@(#)diskconf.c	1.1	96/02/16 15:45:53 */
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

ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  unittype,
				capacity, read, write, ctrl, geometry */

    { "floppy", flp_devinit, flp_unitinit, flp_unittype,
			flp_capacity, flp_read, flp_write, flp_control, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] =
{
    /*  ctlr_type,	dev_addr,	ivec,	minunit maxunit	*/

    {   0,		0x3F2,		6,	0,	2	},
    {   0,		0,		0,	0,	0	}
};
