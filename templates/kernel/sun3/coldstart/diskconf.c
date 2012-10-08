/*	@(#)diskconf.c	1.7	96/02/16 15:48:26 */
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
#include "memaccess.h"
#include "machdep.h"
#include "map.h"
#include "scsi_sun3.h"
#include "disk/conf.h"
#include "ramdisk.h"
#include "generic/sd.h"


ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  capacity, read, write, control, geometry */

    { "scsi", scsi_ctlr_init, sd_unit_init, scsi_unit_type,
				sd_capacity, sd_read, sd_write, 0, 0 },
    { "ramdisk", rd_devinit, rd_unit_init, rd_unit_type,
				rd_capacity, rd_read, rd_write, 0, 0 },
};

/* this table must be null terminated */
confrec	Conf_tab[] =
{
    /*  ctlr_type,	dev_addr,	ivec,		maxunits	*/

    {   0,		0,		26,		0,17	},
    {	1,		0,		0,		0,1	},
    {   0,		0,		0,		0,0	}
};

/*
** Where to find the scsi device and its corresponding dma and control
** registers
*/
struct scsi_info	Scsi_info[SUN_NUM_SCSI_CONTROLLERS] = {
    {
	(IOBASE + 0x14000),
	(IOBASE + 0x14010),
	(IOBASE + 0x14014),
	0xF00000,
    }
};

static long rdsizes[] = { 4100, 200, 100 };

extern errstat tape_loader();
struct ram_parameters ram_pars[] = {
    {	4401,		tape_loader,	"/tape:00",	1, 3, rdsizes	}
};
