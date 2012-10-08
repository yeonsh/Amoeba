/*	@(#)b_size.c	1.2	94/04/07 11:00:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_size
**	Bullet server client stub: gets the size of a file.
**	The return value is the error status.  The result is returned via
**	the parameter 'size'.
**	'size' is not altered if an error occurred.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

errstat
b_size(cap, size)
capability *	cap;	/* in: capability of the file to be sized */
b_fsize *	size;	/* out: size of file */
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = BS_SIZE;
    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(n);
    if (hdr.h_status == STD_OK)
	*size = hdr.h_offset;
    return ERR_CONVERT(hdr.h_status);
}
