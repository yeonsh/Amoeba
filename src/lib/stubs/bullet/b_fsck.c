/*	@(#)b_fsck.c	1.2	94/04/07 10:59:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_fsck
**	Bullet Server administrator stub routine for file system check.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "bullet/bullet.h"

errstat
b_fsck(supercap)
capability *	supercap;	/* in: super capability for bullet server */
{
    header	hdr;
    bufsize	n;

    hdr.h_port = supercap->cap_port;
    hdr.h_priv = supercap->cap_priv;
    hdr.h_command = BS_FSCK;
    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(n);
    return ERR_CONVERT(hdr.h_status);
}
