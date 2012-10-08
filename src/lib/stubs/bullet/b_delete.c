/*	@(#)b_delete.c	1.3	96/02/27 11:14:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_delete
**	The bullet server "delete" client stub.
**	Has a problem because size may not fit into the hdr.h_size field.
**	For now we put the size in the buffer but in AMOEBA4 it can go in
**	the header.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/buffers.h"

#define	BFSZ	(2 * sizeof (b_fsize)) /* should be enough space */

errstat
b_delete(cap, offset, size, commit, newfile)
capability *	cap;		/* in: cap for the bullet file to modify */
b_fsize		offset;		/* in: where to make the change */
b_fsize		size;		/* in: #bytes to delete */
int		commit;		/* in: the commit and safety flags */
capability *	newfile;	/* out: capability for created file */
{
    header	hdr;
    bufsize	t;
    char	buf[BFSZ];	/* need to send a 2nd long so use a buffer */
    char *	p;

    if (size < 0)
	return STD_ARGBAD;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_offset = offset;
    hdr.h_command = BS_DELETE;
    hdr.h_extra = commit;
    if ((p = buf_put_bfsize(buf, &buf[BFSZ], size)) == 0)
	return STD_ARGBAD;
    if ((t = trans(&hdr, buf, (bufsize) (p - buf), &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(t);
    if (hdr.h_status != STD_OK)
	return ERR_CONVERT(hdr.h_status);

/* give the user the new capability */
    newfile->cap_port = hdr.h_port;
    newfile->cap_priv = hdr.h_priv;
    return STD_OK;
}
