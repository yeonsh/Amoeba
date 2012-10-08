/*	@(#)dkl_misc.c	1.3	96/02/27 14:13:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	There are several routines used for managing disk labels
**	that are used by the disk labelling utility and the disk
**	servers.  This routines appear here.
**
**  Greg Sharp
*/

#include "amoeba.h"
#include "byteorder.h"
#include "disk/disk.h"
#include "vdisk/disklabel.h"

/*
** AM_PARTN_LABEL
**
**	Returns 1 if the disk has a reasonable looking amoeba partition label
**	and 0 otherwise.
*/

am_partn_label(label)
disklabel *	label;
{
    uint16	checksum();

    return (label->d_magic == AM_DISK_MAGIC && checksum((uint16 *) label) == 0);
}


/*
** TOTAL_BLOCKS
**
**	Returns the total number of blocks in use as given by the Amoeba
**	partition table.
*/

disk_addr
total_blocks(ptab)
part_tab *	ptab;
{
    int 	i;
    disk_addr	sum;

    for (sum = 0, i = 0; i < MAX_PARTNS_PDISK; ptab++, i++)
	sum += ptab->p_numblks;
    return sum;
}
