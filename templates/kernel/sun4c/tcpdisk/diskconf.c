/*	@(#)diskconf.c	1.1	96/02/16 15:50:38 */
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
#include "disk/conf.h"
#include "interrupt.h"
#include "scsi_sun4.h"
#include "sd.h"
#include "../sun4gen/sun4_floppy.h"

ctlrtype Cntlrtab[] =
{
    /* name,  devinint,  unitinit,  capacity, read, write, control, geometry */

    { "scsi", scsi_ctlr_init, sd_unit_init, scsi_unit_type,
				sd_capacity, sd_read, sd_write, 0, 0 },
};


/*
 * This table shows the information about the controllers which is not
 * specific to a particular controller type.
 * This table must be null terminated.
 */

confrec	Conf_tab[] =
{
    /*  ctlr_type,	dev_addr,	ivec,		maxunits	*/

    {   0,		0,	 INT_SCSI,		0,32	},
    {   0,		0,		0,		0,0	}
};
