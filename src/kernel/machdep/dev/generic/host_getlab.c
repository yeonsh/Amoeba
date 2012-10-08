/*	@(#)host_getlab.c	1.3	96/02/27 13:48:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "disk/disk.h"
#include "vdisk/disklabel.h"

/*
** HOST_GETLABEL
**
**      This routine is the generic host_getlabel.  Its fundtion is to
**	pretend that there was no host label for the case where no host
**	label is possible/meaningfule, such as ram disk file systems.
**      Returns 1 if the label was invalid.
*/

host_getlabel(dk_read, devaddr, unit, h_label, am_label)
errstat		(*dk_read)();	/* read routine for disk */
long		devaddr;	/* device address of disk controller */
int		unit;	 	/* unit number of disk to read from */
char *		h_label;	/* buffer to read host OS label into */
disklabel *	am_label;	/* amoeba label */
{
    return 1;
}


host_partn_offset(label)
char *	label;
{
    return 0;
}
