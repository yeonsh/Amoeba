/*	@(#)b_sync.c	1.2	94/04/07 11:00:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_sync
**	Bullet server client stub: flushes committed files to disk
**	The return value is the error status.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

errstat
b_sync(cap)
capability *	cap;	/* in: super capability for bullet server */
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = BS_SYNC;
    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(n);
    return ERR_CONVERT(hdr.h_status);
}
