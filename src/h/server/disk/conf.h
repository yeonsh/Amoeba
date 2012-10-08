/*	@(#)conf.h	1.5	96/02/27 10:36:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DISK_CONF_H__
#define __DISK_CONF_H__

/*
** conf.h
**
**	This file contains the data structure for auto-configuring devices
**	under Amoeba.
**	The current stuff is for auto-configuring the disks but it could
**	be expanded to handle uarts and other things that might be about.
**
**  Author:
**	Greg Sharp
*/

#include "disk/disk.h"
#include "vdisk/disklabel.h"


typedef	struct confrec		confrec;	/* configuration record entry */
typedef struct ctlrtype		ctlrtype;	/* table of controller types */
typedef struct part_tp		part_tp;

/*
** Configuration Table Record Structure.
**
**	For each type of device we need to know what type of controller
**	it has and the address of the device so that we can access it.
**	The device need not necessarily exist there since we can test to
**	see if it is there.
**
** c_ctlr_type:	Index into disk controller table.  This needs to be
**		geberalised eventually.
** c_dev_addr:	Address where kernel can probably find a device of
**		the above type.
** c_ivec:	Interrupt vector for this device.
** c_minunit:	Minimum unit number to start looking for disks on this
**		controller.
** c_maxunit:	Maximum unit number for which we look for disks on this
**		controller.
*/

struct confrec
{
    int		c_ctlr_type;
    long	c_dev_addr;
    int		c_ivec;
    int		c_minunit;
    int		c_maxunit;
};


/*
** Controller Type Structure.
**
**	For each type of disk controller (scsi, smd, etc.) we need routines
**	for initialising, reading and writing the device.
**
** c_name[]:	Null terminated string describing the controller type for
**		user-interface programs.
** c_devinit:	Pointer to routine to initialise the controller hardware.
** c_unitinit:	Pointer to routine to initialise the individual disks.
** c_unittype:	Pointer to routine to determine the type of individual disks.
** c_read:	Pointer to routine to read from the controller.
** c_write:	Pointer to routine to write to the controller.
** c_ctl:	Pointer to routine to execute control commands (eg. eject)
** c_geometry:	Pointer to routine to provide disk geometry info.
*/

struct ctlrtype
{
    char	c_name[7];
    errstat	(*c_devinit) _ARGS(( long devaddr, int ivec ));
    errstat	(*c_unitinit) _ARGS(( long devaddr, int unit ));
    int		(*c_unittype) _ARGS(( long devaddr, int unit ));
    errstat	(*c_capacity) _ARGS(( long devaddr, int unit, int32 * lastblk,
				      int32 * blksz ));
    errstat	(*c_read) _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				  disk_addr blk_cnt, char * buf ));
    errstat	(*c_write) _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				   disk_addr blk_cnt, char * buf ));
    errstat	(*c_ctl) _ARGS(( long devaddr, int unit, int cmd, bufptr buf,
				 bufsize size ));
    errstat	(*c_geometry) _ARGS(( long devaddr, int unit, geometry *geom ));
};


/*
** Types returned by (*c_unittype)().
*/
#define	UNITTYPE_FIXED		1	/* fixed disk (e.g winchester) */
#define	UNITTYPE_REMOVABLE	2	/* removable disk (e.g. floppy) */

/*
** For the ram disk driver.
**
** part_size:	The size of the ram disk partition (in bytes?).
** part_name:	Character string with name of ram disk partition.
*/

struct part_tp
{
    int32	part_size;
    char *	part_name;
};

#endif /* __DISK_CONF_H__ */
