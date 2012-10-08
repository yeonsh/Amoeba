/*	@(#)b_disk_comp.c	1.2	94/04/07 10:59:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_disk_compact
**	Bullet server client stub: requests that the disk fragmentation
**	be eliminated.
**	The return value is the error status.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

errstat
b_disk_compact(cap)
capability *	cap;	/* in: super capability of the bullet server */
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = BS_DISK_COMPACT;
    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(n);
    return ERR_CONVERT(hdr.h_status);
}
