/*	@(#)diskdep.c	1.4	96/02/27 13:52:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	This file contains the host operating system dependencies for
**	dealing with disks.  Primarily they are concerned with the disk
**	label of SUN OS.  They read/verify labels and copy the information
**	about the disk into a struct known by Amoeba.
*/

#include <string.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "disk/disk.h"
#include "vdisk/disklabel.h"
#include "vdisk/disktype.h"
#include "sunlabel.h"

#define	MAX(a, b)	((a) > (b) ? (a) : (b))

/*
** HOSTLABEL
**
**	Routines to support host operating system dependencies which affect
**	the disk driver.  Returns 1 for a valid label and 0 otherwise.
*/

host_label(label)
char *	label;
{
    uint16	checksum();
    sun_label *	slp;

    slp = (sun_label *) label;
    return slp->sun_magic == SUN_MAGIC && checksum((uint16 *) slp) == 0;
}


/*
** HOST_GETLABEL
**
**	This routine reads a sun OS disk label and puts the partition and
**	geometry info into an amoeba label.  Returns -1 if the read failed.
**	Returns 1 if the label was invalid.  Returns 0 on success.
*/

host_getlabel(dk_read, devaddr, unit, label, am_label)
errstat		(*dk_read)();	/* read routine for disk */
long		devaddr;	/* device address of disk controller */
int		unit;		/* unit number of disk to read from */
char *		label;		/* optional return buffer for the label */
disklabel *	am_label;	/* amoeba label */
{
    int		host_label();
    void	copy_sun_amoeba();

    char	h_label[D_PHYSBLKSZ];	/* buffer to read host OS label into */

/* read the sun OS label */
    if ((*dk_read)(devaddr, unit, 0, 1, h_label) != STD_OK)
	return -1;

/* check it is a valid label */
    if (!host_label(h_label))
	return 1;
/* fill in amoeba label */
    am_label->d_disktype[0] = '\0';	/* unknown */
    copy_sun_amoeba((sun_label *) h_label, am_label);
    if (label != 0)
	(void) memmove((_VOIDSTAR) label,
		       (_VOIDSTAR) h_label, (size_t) D_PHYSBLKSZ);
    return 0;
}


/*
** COPY_SUN_AMOEBA
**
**	Copy data from a sun label to an amoeba label where the data
**	in some way corresponds.
*/

void
copy_sun_amoeba(slp, am_label)
sun_label *	slp;
disklabel *	am_label;
{
    uint16	checksum();

    int		i;

    am_label->d_geom.g_bpt = 0;		/* unknown */
    am_label->d_geom.g_bps = 0;		/* unknown */

    am_label->d_geom.g_numcyl = slp->sun_numcyl;
    am_label->d_geom.g_altcyl = slp->sun_altcyl;
    am_label->d_geom.g_numhead = slp->sun_numhead;
    am_label->d_geom.g_numsect = slp->sun_numsect;

/*
** when copying partition table we musn't copy more than there is or more
** than we can hold
*/
    i = MAX(SUN_MAX_PARTNS, MAX_PARTNS_PDISK);
    while (--i >= 0)
    {
	am_label->d_ptab[i].p_firstblk = slp->sun_map[i].sun_cylno * slp->sun_numhead * slp->sun_numsect;
	am_label->d_ptab[i].p_numblks = slp->sun_map[i].sun_nblk;
    }
    am_label->d_magic = AM_DISK_MAGIC;
    am_label->d_chksum = 0;
    am_label->d_chksum = checksum((uint16 *) am_label);
}


/*
** HOST_PARTN_OFFSET
**
**	This is VERY IMPORTANT.  This tells the system how big the boot
**	partition on the disk is.  Usually it is the first cylinder of the
**	disk but some OS's use more!  The calling code assumes that the
**	boot info starts at cylinder 0.  If this isn't the case then some
**	work will need to be done in disklabel.c
**	Returns:  number of SECTORS reserved for boot block!
*/

disk_addr
host_partn_offset(amlabel)
disklabel *	amlabel;
{
    return amlabel->d_geom.g_numhead * amlabel->d_geom.g_numsect;
}
