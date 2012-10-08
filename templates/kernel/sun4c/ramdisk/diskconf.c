/*	@(#)diskconf.c	1.4	96/02/16 15:51:44 */
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
#include "machdep.h"
#include "map.h"
#include "disk/conf.h"
#include "ramdisk.h"


ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  capacity, read, write, control, geometry */

    { "ramdisk", rd_devinit, rd_unit_init, rd_unit_type,
				rd_capacity, rd_read, rd_write, 0, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] =
{
    /*  ctlr_type,	dev_addr,	ivec,		maxunits	*/

    {   0,		0,		0,		0,1	},
    {   0,		0,		0,		0,0	}
};

static long rdsizes[] = { 4500, 200, 100 };

struct ram_parameters ram_pars[] = {
    {	4801,	0,	"dummy",	0, 3, rdsizes	}
};
