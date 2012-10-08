/*	@(#)b_modify.c	1.2	94/04/07 10:59:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** b_modify
**	The bullet server "modify file" client stub.
**	If the data in buf won't fit in one transaction then we have
**	do several modify commands.
**	We must not send "commit" until the last transaction.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "bullet/bullet.h"

errstat
b_modify(cap, offset, buf, size, commit, newfile)
capability *	cap;		/* in: cap for the bullet file to modify */
b_fsize		offset;		/* in: where to make the change */
char *		buf;		/* in: new data to modify the file */
b_fsize		size;		/* in: #bytes data in buf */
int		commit;		/* in: the commit and safety flags */
capability *	newfile;	/* out: capability for created file */
{
    header	hdr;
    bufsize	t;
    b_fsize	sendsize;

    if (size < 0)
	return STD_ARGBAD;

    hdr.h_port = cap->cap_port;		/* Initial cap.  May be changed by BS */
    hdr.h_priv = cap->cap_priv;		/* to new file cap! So do this here! */
    hdr.h_offset = offset;
    do
    {
	hdr.h_command = BS_MODIFY;
	if (size > BS_REQBUFSZ)		/* not last transaction */
	{
	    hdr.h_size = BS_REQBUFSZ;
	    hdr.h_extra = 0;
	}
	else	/* last transaction so send commit flags */
	{
	    hdr.h_size = size;
	    hdr.h_extra = commit;
	}
	sendsize = hdr.h_size;
	if ((t = trans(&hdr, buf, hdr.h_size, &hdr, NILBUF, 0)) != 0)
	    return ERR_CONVERT(t);
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	buf += sendsize;
	hdr.h_offset += sendsize;
    } while ((size -= sendsize) > 0);

/* give the user the new capability */
    newfile->cap_port = hdr.h_port;
    newfile->cap_priv = hdr.h_priv;
    return STD_OK;
}
