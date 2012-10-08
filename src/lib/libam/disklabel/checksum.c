/*	@(#)checksum.c	1.4	96/02/27 11:01:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"


/*
** CHECKSUM
**	Returns the exclusive or of all the words in the given label.
**	When generating the checksum for a new label the d_chksum field
**	should be set to 0 before this operation.
**	When this routine is called with an "old" label it should return 0
**	if the checksum in the label is correct.
*/

uint16
checksum(p)
uint16 *	p;	/* pointer to label to be checksumed */
{
    int		i;
    uint16	sum;

    i = sizeof (disklabel) / sizeof (uint16);
    sum = 0;
    while (--i >= 0)
	sum = sum ^ *p++;
    return sum;
}
